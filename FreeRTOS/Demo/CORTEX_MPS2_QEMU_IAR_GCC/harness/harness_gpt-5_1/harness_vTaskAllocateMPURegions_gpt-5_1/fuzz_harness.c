/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: vTaskAllocateMPURegions_basic_fuzz
 * 生成时间: 2025-12-18 20:29:57
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: vTaskAllocateMPURegions_basic_fuzz
 * 描述: Fuzz vTaskAllocateMPURegions by creating a restricted task with static MPU regions, then repeatedly reallocating MPU regions for either the created task or the calling task using parameters derived from FUZZ_INPUT.
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

// LibAFL断点函数
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
// Fuzz Reader (FR_*)
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
    return (hi << 16) | lo;
}
static inline int32_t FR_next_s32(FR_Reader* r) { return (int32_t)FR_next_u32(r); }
static inline uint32_t FR_next_range(FR_Reader* r, uint32_t min_v, uint32_t max_v) {
    if (max_v <= min_v) return min_v;
    uint32_t span = max_v - min_v + 1u;
    return min_v + (FR_next_u32(r) % span);
}
static inline size_t FR_next_bytes(FR_Reader* r, unsigned char* out, size_t n) {
    size_t rem = FR_remaining(r); if (n > rem) n = rem; if (n) { memcpy(out, r->data + r->off, n); r->off += n; } return n;
}
static inline size_t FR_next_string(FR_Reader* r, char* out, size_t max_len) {
    if (!out || max_len == 0) {
        return 0;
    }
    out[0] = '\0';
    if (max_len == 1) {
        return 0;
    }
    size_t max_copy = max_len - 1;
    size_t rem = FR_remaining(r);
    if (rem == 0 || max_copy == 0) {
        return 0;
    }
    size_t span = rem < max_copy ? rem : max_copy;
    size_t len = span ? (FR_next_u16(r) % (span + 1)) : 0;
    if (len == 0) {
        return 0;
    }
    size_t got = FR_next_bytes(r, (unsigned char*)out, len);
    out[got] = '\0';
    return got;
}

// ====================================================================
// 本测试用例的辅助数据结构
// ====================================================================
#ifndef TEST_TASK_STACK_SIZE
#define TEST_TASK_STACK_SIZE 256U
#endif

/* Static storage for a "restricted" task and its stack. */
static StaticTask_t xRestrictedTCB;
static StackType_t xRestrictedStack[ TEST_TASK_STACK_SIZE ];
static TaskHandle_t xRestrictedTaskHandle = NULL;
static BaseType_t xRestrictedTaskCreated = pdFALSE;

/* Provide a couple of statically allocated buffers that can be used as MPU regions. */
__attribute__((aligned(128))) static uint8_t ucRegionBuf1[128];
__attribute__((aligned(256))) static uint8_t ucRegionBuf2[256];

/* Simple task function used when we create the extra task. */
static void vRestrictedTask( void * pvParameters )
{
    ( void ) pvParameters;
    /* Do a small amount of work then delete self. */
    taskYIELD();
    vTaskDelete( NULL );
}

/*
 * Helper to lazily create a task that we can pass as the target handle to
 * vTaskAllocateMPURegions(). On this non-MPU demo port, vTaskAllocateMPURegions
 * may not be available, so we only create a normal static task and never touch
 * MPU region attributes.
 */
static void prvEnsureRestrictedTaskCreated( void )
{
    if( xRestrictedTaskCreated == pdFALSE )
    {
        xRestrictedTaskHandle = xTaskCreateStatic(
            vRestrictedTask,
            "RTask",
            TEST_TASK_STACK_SIZE,
            NULL,
            tskIDLE_PRIORITY + 1,
            xRestrictedStack,
            &xRestrictedTCB );

        if( xRestrictedTaskHandle != NULL )
        {
            xRestrictedTaskCreated = pdTRUE;
        }
    }
}

// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void *pvParameters)
{
    (void) pvParameters;

    // 统一使用 FUZZ_INPUT 构造 Reader
    FR_Reader fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);

    unsigned char fr_baseline[16] = {0};
    (void)FR_next_bytes(&fr, fr_baseline, sizeof(fr_baseline));

    unsigned int iterations = (unsigned int)FR_next_range(&fr, 0, 10);

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        printf("[ERROR] Scheduler not running - aborting test_task\n");
        fflush(stdout);
        vTaskDelete(NULL);
    }

    for (unsigned int iter = 0; iter < iterations; ++iter) {
        /* Ensure we have a secondary task handle available. */
        prvEnsureRestrictedTaskCreated();

        if( FR_remaining( &fr ) < 4 ) {
            break;
        }

        /* Decide whether to target the created task or the calling task (NULL). */
        uint8_t targetSel = FR_next_u8(&fr);
        TaskHandle_t xTargetHandle = NULL;
        if( ( targetSel & 0x1u ) != 0u ) {
            xTargetHandle = xRestrictedTaskHandle;
        }

        /* Build a small set of regions only if MPU support is enabled. */
        #if( portUSING_MPU_WRAPPERS == 1 )
        MemoryRegion_t xRegions[ portNUM_CONFIGURABLE_REGIONS ];

        for( size_t r = 0; r < portNUM_CONFIGURABLE_REGIONS; r++ ) {
            uint8_t baseSel = FR_next_u8(&fr);
            uint8_t lenByte = FR_next_u8(&fr);

            void * pvBase = NULL;
            uint32_t ulLen = 0U;

            switch( baseSel % 3U ) {
                case 0:
                    pvBase = ucRegionBuf1;
                    ulLen  = sizeof( ucRegionBuf1 );
                    break;
                case 1:
                    pvBase = ucRegionBuf2;
                    ulLen  = sizeof( ucRegionBuf2 );
                    break;
                default:
                    pvBase = NULL;
                    ulLen  = (uint32_t)lenByte;
                    break;
            }

            xRegions[r].pvBaseAddress   = pvBase;
            xRegions[r].ulLengthInBytes = ulLen;
            xRegions[r].ulParameters    = 0U; /* No MPU attribute flags on this port. */
        }

        /* Call the API under test when it is available. */
        vTaskAllocateMPURegions( xTargetHandle, xRegions );
        #else
        /* On non-MPU builds, just consume a couple of bytes to keep
         * the fuzzing pattern similar but do not call the API that
         * is not linked in this configuration. */
        (void)FR_next_u8(&fr);
        (void)FR_next_u8(&fr);
        (void)xTargetHandle;
        #endif

        taskYIELD();
    }

    printf("[TEST_CASE_COMPLETED]\n");
    fflush(stdout);

    BREAKPOINT();
}

// FreeRTOS任务包装器
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

    if (xHandle == NULL) {
        printf("FuzzTask creation failed\n");
        fflush(stdout);
        configASSERT(0);
        for (;;) {
        }
    }

    vTaskStartScheduler();

    for (;;) {
    }
}
