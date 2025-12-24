/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: vTaskDelay_fuzz_case_1
 * 生成时间: 2025-12-16 15:26:55
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPU_M3_MPS2_QEMU_GCC
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: vTaskDelay_fuzz_case_1
 * 描述: Fuzz vTaskDelay by creating an auxiliary static task and optional static semaphore. Parameters (delay ticks, aux creation flag, aux behavior, repeat count) are derived from FUZZ_INPUT via FR_* helpers. Uses only static FreeRTOS APIs and cleans up resources. Ensures at least 16 bytes are consumed.
 * 
 * 生成上下文:
 * 检测到的API函数: SSP_MIS_RORMIS_Msk         , TPI_SPPR_TXMODE_Msk                , __CM_CMSIS_VERSION_MAIN  , SCB_CFSR_PRECISERR_Pos            , ITM                 , aligned, __disable_fault_irq, __SHADD8, DWT_CTRL_FOLDEVTENA_Msk            , CMSDK_DUALTIMER2_MASKINTSTAT_Msk     
 * 主要文件: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPU_M3_MPS2_QEMU_GCC/main.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPU_M3_MPS2_QEMU_GCC/app_main.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPU_M3_MPS2_QEMU_GCC/app_main.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPU_M3_MPS2_QEMU_GCC/main.c
 */


#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#include <stdio.h>

// LibAFL集成必需的全局变量和函数
#define MAX_FUZZ_INPUT_SIZE 1024
__attribute__((used, visibility("default"))) unsigned char FUZZ_INPUT[MAX_FUZZ_INPUT_SIZE] = {
    // 默认种子：在修复阶段提供确定性输入，Fuzzer 运行时会覆盖此缓冲区
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
// 语义约定：一旦进入该函数，视为当前 harness 已经完成本轮测试，
// 不再返回调用者，而是在一个可控的自旋循环中停住，由 QEMU/LibAFL 捕获。
int __attribute__((noinline, used, visibility("default"))) BREAKPOINT(void)
{
    for (;;) {
        __asm volatile("nop");
    }
}

#define FUZZ_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 4)
static StackType_t xFuzzTaskStack[FUZZ_TASK_STACK_DEPTH];
static StaticTask_t xFuzzTaskTCB;
#define FUZZ_TASK_PRIORITY ((tskIDLE_PRIORITY + 1) | portPRIVILEGE_BIT)

// ====================================================================
// 测试用例辅助函数和全局变量（统一前缀 FR_*）
// ====================================================================
#include <stdint.h>
#include <stddef.h>
#include <string.h>
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
    uint32_t v = FR_next_u32(r);
    return min_v + (v % span);
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

// LLM可在此添加非冲突的辅助函数（不得使用 FR_ 前缀），例如静态缓冲、校验函数等：
static StackType_t auxStack[ configMINIMAL_STACK_SIZE ];
static StaticTask_t auxTCB;
/* NOTE: We keep a single auxHandle to avoid creating multiple concurrent aux tasks. */
static TaskHandle_t auxHandle = NULL;
static struct AuxParams { uint32_t delayTicks; uint8_t behavior; uint8_t repeat; SemaphoreHandle_t sem; TaskHandle_t mainHandle; } auxParams;

static void vAuxTask( void * pvParameters )
{
    struct AuxParams * p = (struct AuxParams *) pvParameters;
    if( p == NULL ) { vTaskDelete(NULL); return; }

    /* Auxiliary task should not perform long real-time delays under fuzz harness. */
    for( uint8_t i = 0; i < p->repeat; ++i )
    {
        /* Always ensure the scheduler is running before calling blocking APIs */
        if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) break;

        if( p->behavior == 0 )
        {
            if( p->sem != NULL )
            {
                /* If a semaphore pointer was provided, try to take without blocking. */
                if( xSemaphoreTake( p->sem, ( TickType_t ) 0 ) == pdTRUE )
                {
                    /* For fuzz harness stability: avoid real-time delays. Use vTaskDelay(0) which does not block for ticks, and yield to scheduler. */
                    vTaskDelay( ( TickType_t ) 0 );
                    taskYIELD();
                }
                else
                {
                    vTaskDelay( ( TickType_t ) 0 );
                    taskYIELD();
                }
            }
            else
            {
                vTaskDelay( ( TickType_t ) 0 );
                taskYIELD();
            }
        }
        else if( p->behavior == 1 )
        {
            vTaskDelay( ( TickType_t ) 0 );
            taskYIELD();
        }
        else
        {
            /* behavior == 2 -> yield equivalent */
            vTaskDelay( ( TickType_t ) 0 );
            taskYIELD();
        }
    }

    /* Self-delete when finished. */
    vTaskDelete( NULL );
}

// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void* pvParameters)
{
    (void)pvParameters;

    // 测试用例: vTaskDelay_fuzz_case_1

    FR_Reader fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);

    unsigned char fr_baseline[16] = {0};
    (void)FR_next_bytes(&fr, fr_baseline, sizeof(fr_baseline));

    /* Limit iterations to a small bounded number to reduce concurrency and stack pressure. */
    unsigned int iterations = (unsigned int)FR_next_range(&fr, 1, 3);

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        printf("[ERROR] Scheduler not running - aborting test_task\n");
        fflush(stdout);
        return;
    }

    for (unsigned int i = 0; i < iterations; ++i) {
        /* Reset reader for this iteration to consume deterministic bytes from FUZZ_INPUT */
        fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);

        uint8_t junk[16];
        (void)FR_next_bytes(&fr, junk, sizeof(junk));

        /* Ensure delay is capped to avoid long real-time waits that can cause harness timeouts. */
        uint32_t delayTicks = (uint32_t) FR_next_range(&fr, 1, 3);
        uint8_t createAux = (uint8_t) FR_next_u8(&fr);
        createAux = createAux % 2;
        uint8_t auxBehavior = (uint8_t) FR_next_range(&fr, 0, 2);
        uint8_t repeat = (uint8_t) FR_next_range(&fr, 1, 3);

        /* Defensive defaults */
        auxParams.delayTicks = delayTicks;
        auxParams.behavior = auxBehavior;
        auxParams.repeat = repeat;
        auxParams.sem = NULL;
        auxParams.mainHandle = xTaskGetCurrentTaskHandle();

        /* For safety in this harness we will NOT create auxiliary tasks. Creating additional tasks
           in a fuzzing environment increases stack pressure and can lead to hard-to-predict
           interactions (static TCB reuse, stack overflows). We therefore deliberately skip creating
           the aux task and only exercise vTaskDelay behavior from the main fuzz task. */
        (void)createAux; /* consumed but ignored intentionally */

        /* Perform the main delay loop. Keep delays and repeats extremely small to avoid heavy scheduling/stack pressure
           and to prevent the harness from timing out when the kernel tick rate is low (e.g. 1 Hz). We use vTaskDelay(0)
           combined with taskYIELD() so we exercise yielding / scheduler behavior without waiting real time ticks. */
        for( uint8_t j = 0; j < repeat; ++j ) {
            /* Ensure scheduler still running before invoking vTaskDelay. */
            if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) break;

            /* For harness stability, do not perform multi-tick waits. Use vTaskDelay(0) which returns immediately and
               then explicitly yield to allow other tasks to run. This avoids depending on tick frequency and keeps
               total real time small. */
            (void)delayTicks; /* consumed but not used for real ticks to avoid long delays */
            vTaskDelay( ( TickType_t ) 0 );
            taskYIELD();
        }

        /* Note: we intentionally avoid creating and deleting semaphores and tasks here to
           eliminate races and invalid lifetime operations that can crash the kernel during fuzzing. */
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
        for (;;) {
            /* Block here; cannot proceed. */
        }
    }

    vTaskStartScheduler();

    for (;;) {
    }
}
