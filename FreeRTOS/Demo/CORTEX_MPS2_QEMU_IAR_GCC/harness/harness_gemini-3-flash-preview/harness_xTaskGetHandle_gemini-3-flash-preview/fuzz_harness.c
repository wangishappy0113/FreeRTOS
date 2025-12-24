/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: fuzz_xTaskGetHandle_lifecycle
 * API类别: task_management
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// LibAFL集成必需的全局变量和函数
#define MAX_FUZZ_INPUT_SIZE 1024
__attribute__((used, visibility("default"))) unsigned char FUZZ_INPUT[MAX_FUZZ_INPUT_SIZE] = {
    0x46, 0x55, 0x5a, 0x5a, 0x01, 0x23, 0x45, 0x67,
    0x89, 0xab, 0xcd, 0xef, 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc,
    0xdd, 0xee, 0xff, 0x00, 0x13, 0x37, 0x42, 0x24,
    0x5a, 0xa5, 0xc3, 0x3c, 0xde, 0xed, 0xbe, 0xef,
    0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
    0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x0f,
    0x1f, 0x2f, 0x3f, 0x4f, 0x5f, 0x6f, 0x7f, 0x8f
};

int __attribute__((noinline, used, visibility("default"))) BREAKPOINT(void)
{
    for (;;) {
        __asm volatile("nop");
    }
}

#define FUZZ_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 4)
static StackType_t xFuzzTaskStack[FUZZ_TASK_STACK_DEPTH];
static StaticTask_t xFuzzTaskTCB;
#define FUZZ_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

// ====================================================================
// 测试用例辅助函数和全局变量（统一前缀 FR_*）
// ====================================================================
typedef struct {
    const unsigned char* data;
    size_t size;
    size_t off;
} FR_Reader;

#define FR_init_SELECT(_1,_2,NAME,...) NAME
#define FR_init(...) FR_init_SELECT(__VA_ARGS__, FR_init_2, FR_init_1)(__VA_ARGS__)
static inline FR_Reader FR_init_1(const unsigned char* buf) { FR_Reader r = { buf, MAX_FUZZ_INPUT_SIZE, 0 }; return r; }
static inline FR_Reader FR_init_2(const unsigned char* buf, size_t n) { FR_Reader r = { buf, n, 0 }; return r; }
static inline size_t FR_remaining(FR_Reader* r) { return (r->off < r->size) ? (r->size - r->off) : 0; }
static inline uint8_t FR_next_u8(FR_Reader* r) {
    if (r->off + 1 <= r->size) return r->data[r->off++];
    return 0;
}
static inline uint16_t FR_next_u16(FR_Reader* r) {
    uint16_t lo = FR_next_u8(r);
    uint16_t hi = FR_next_u8(r);
    return (uint16_t)((hi << 8) | lo);
}
static inline uint32_t FR_next_u32(FR_Reader* r) {
    uint32_t lo = FR_next_u16(r);
    uint32_t hi = FR_next_u16(r);
    return (uint32_t)((hi << 16) | lo);
}
static inline uint32_t FR_next_range(FR_Reader* r, uint32_t min_v, uint32_t max_v) {
    if (max_v <= min_v) return min_v;
    uint32_t span = max_v - min_v + 1u;
    return min_v + (FR_next_u32(r) % span);
}
static inline size_t FR_next_bytes(FR_Reader* r, unsigned char* out, size_t n) {
    size_t rem = FR_remaining(r); if (n > rem) n = rem; if (n) { memcpy(out, r->data + r->off, n); r->off += n; } return n;
}
static inline size_t FR_next_string(FR_Reader* r, char* out, size_t max_len) {
    if (!out || max_len == 0) return 0;
    out[0] = '\0';
    if (max_len == 1) return 0;
    size_t max_copy = max_len - 1;
    size_t rem = FR_remaining(r);
    if (rem == 0) return 0;
    size_t len = FR_next_u16(r) % (rem + 1);
    if (len > max_copy) len = max_copy;
    size_t got = FR_next_bytes(r, (unsigned char*)out, len);
    out[got] = '\0';
    return got;
}

static StaticTask_t xStaticTCBs[2];
static StackType_t uxStaticStacks[2][configMINIMAL_STACK_SIZE];
static TaskHandle_t xTaskHandles[2] = { NULL, NULL };

static void vDummyFuzzTask( void * pvParameters ) {
    ( void ) pvParameters;
    for( ;; ) {
        vTaskDelay( pdMS_TO_TICKS( 100 ) );
    }
}

void test_task(void *pvParameters)
{
    (void)pvParameters;
    FR_Reader fr_local = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);
    unsigned char fr_baseline[16] = {0};
    (void)FR_next_bytes(&fr_local, fr_baseline, sizeof(fr_baseline));
    unsigned int iterations = (unsigned int)FR_next_range(&fr_local, 0, 10);

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return;
    }

    for (unsigned int i = 0; i < iterations; ++i) {
        if( FR_remaining( &fr_local ) < ( configMAX_TASK_NAME_LEN * 3 + 10 ) ) {
            break;
        }

        for( int j = 0; j < 2; j++ ) {
            if( xTaskHandles[j] != NULL ) {
                #if ( INCLUDE_vTaskDelete == 1 )
                vTaskDelete( xTaskHandles[j] );
                #endif
                xTaskHandles[j] = NULL;
            }
        }

        char cName1[configMAX_TASK_NAME_LEN];
        char cName2[configMAX_TASK_NAME_LEN];
        char cSearchTerm[configMAX_TASK_NAME_LEN];

        if (FR_next_string(&fr_local, cName1, sizeof(cName1)) == 0) continue;
        if (FR_next_string(&fr_local, cName2, sizeof(cName2)) == 0) continue;
        FR_next_string(&fr_local, cSearchTerm, sizeof(cSearchTerm));

        UBaseType_t uxPriority1 = ( UBaseType_t ) FR_next_range( &fr_local, tskIDLE_PRIORITY, configMAX_PRIORITIES - 1 );
        UBaseType_t uxPriority2 = ( UBaseType_t ) FR_next_range( &fr_local, tskIDLE_PRIORITY, configMAX_PRIORITIES - 1 );
        uint8_t ucAction = FR_next_u8( &fr_local );

        xTaskHandles[0] = xTaskCreateStatic( vDummyFuzzTask, cName1, configMINIMAL_STACK_SIZE, NULL, uxPriority1, uxStaticStacks[0], &xStaticTCBs[0] );
        if( xTaskHandles[0] == NULL ) continue;

        xTaskHandles[1] = xTaskCreateStatic( vDummyFuzzTask, cName2, configMINIMAL_STACK_SIZE, NULL, uxPriority2, uxStaticStacks[1], &xStaticTCBs[1] );
        if( xTaskHandles[1] != NULL ) {
            if( ( ucAction % 2 ) == 0 ) {
                vTaskSuspend( xTaskHandles[1] );
            }
        }

        #if ( INCLUDE_xTaskGetHandle == 1 )
        (void)xTaskGetHandle( cName1 );
        (void)xTaskGetHandle( cName2 );
        (void)xTaskGetHandle( cSearchTerm );
        #endif

        #if ( INCLUDE_vTaskDelete == 1 )
        if( xTaskHandles[0] != NULL ) { vTaskDelete( xTaskHandles[0] ); xTaskHandles[0] = NULL; }
        if( xTaskHandles[1] != NULL ) { vTaskDelete( xTaskHandles[1] ); xTaskHandles[1] = NULL; }
        #endif
    }

    printf("[TEST_CASE_COMPLETED]\n");
    fflush(stdout);
    BREAKPOINT();
}

void fuzz_task(void)
{
    TaskHandle_t xHandle = xTaskCreateStatic(
        test_task,
        "FuzzTask",
        FUZZ_TASK_STACK_DEPTH,
        NULL,
        FUZZ_TASK_PRIORITY,
        xFuzzTaskStack,
        &xFuzzTaskTCB
    );
    if (xHandle != NULL) {
        vTaskStartScheduler();
    }
    for (;;) {}
}
