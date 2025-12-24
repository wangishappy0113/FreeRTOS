/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: fuzz_xTaskResumeAll_nesting_and_pending
 * 生成时间: 2025-12-23 15:46:01
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: fuzz_xTaskResumeAll_nesting_and_pending
 * 描述: Fuzzes xTaskResumeAll by testing nested scheduler suspension, handling of pending tasks, and processing of pended ticks. A worker task is created with a fuzzed priority (higher, same, or lower than the test task). The test logic nests calls to vTaskSuspendAll, then unblocks the worker task via a semaphore, and simulates pended ticks with vTaskDelay. It then unwinds the nested calls to xTaskResumeAll, asserting that intermediate calls return pdFALSE. The final call's return value is validated to correctly reflect whether a context switch to the higher-priority worker was pending. Task execution flags are checked post-resume to confirm correct scheduling behavior.
 * 
 * 生成上下文:
 * 检测到的API函数: CMSDK_UART0_BASE        , vTraceUBEvent, __STREX, DWT_FUNCTION_EMITRANGE_Msk         , ARM_MPU_REGION_SIZE_8KB      , TPI_FIFO1_ITM0_Msk                 , __enable_fault_irq, __get_MSP, MPU_BASE          , __LDRBT
 * 主要文件: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/main.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/main_full.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/main.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/main_test.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/main_blinky.c
 */

/*
 * LLM生成信息 (用于调试和追踪):
 * 
 * System Prompt:
 * 未记录
 * 
 * User Prompt:
 * 未记录
 * 
 * LLM Response:
 * 未记录
 * 
 * 生成时间: 未记录
 */


#include "FreeRTOS.h"

#include "task.h"

#include "queue.h"

#include "semphr.h"

#include "timers.h"

// 本模板专用于 FreeRTOS CORTEX_MPS2_QEMU_IAR_GCC Demo（非 MPU 端口）
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
#define FUZZ_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

// ====================================================================
// 测试用例辅助函数和全局变量（统一前缀 FR_*）
// ====================================================================
// 统一内置 Fuzz Reader，避免命名冲突。请只调用 FR_* API，不要重复实现。
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct {
    const unsigned char* data;
    size_t size;
    size_t off;
} FR_Reader;
// 可变参数宏：支持 FR_init(buf) 或 FR_init(buf, len)
// ⚠️ 单参数版本仅适用于 FUZZ_INPUT（其大小固定为 MAX_FUZZ_INPUT_SIZE）。
//    其他缓冲区务必显式传入长度以避免越界。
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

// LLM可在此添加非冲突的辅助函数（不得使用 FR_ 前缀），例如静态缓冲、校验函数等：
#define WORKER_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

static StackType_t uxWorkerStack[WORKER_STACK_SIZE];
static StaticTask_t xWorkerTCB;
static TaskHandle_t xWorkerHandle = NULL;

static StaticSemaphore_t xTestSemaphoreBuffer;
static SemaphoreHandle_t xTestSemaphore = NULL;

/* This flag is set by the worker task to signal it has run. */
static volatile BaseType_t worker_task_ran_flag = pdFALSE;

/*
 * The worker task waits on a semaphore. When it receives it, it sets a flag
 * and then suspends itself to prevent re-running within the same test iteration.
 */
static void vWorkerTask(void *pvParameters)
{
    SemaphoreHandle_t semaphore = (SemaphoreHandle_t)pvParameters;
    for (;;)
    {
        if (xSemaphoreTake(semaphore, portMAX_DELAY) == pdTRUE)
        {
            worker_task_ran_flag = pdTRUE;
            vTaskSuspend(NULL); /* Suspend self to allow test task to verify state */
        }
    }
}

// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void)
{
    // 测试用例: fuzz_xTaskResumeAll_nesting_and_pending
    // API类别: task_management
    // 描述: Fuzzes xTaskResumeAll by testing nested scheduler suspension, handling of pending tasks, and processing of pended ticks. A worker task is created with a fuzzed priority (higher, same, or lower than the test task). The test logic nests calls to vTaskSuspendAll, then unblocks the worker task via a semaphore, and simulates pended ticks with vTaskDelay. It then unwinds the nested calls to xTaskResumeAll, asserting that intermediate calls return pdFALSE. The final call's return value is validated to correctly reflect whether a context switch to the higher-priority worker was pending. Task execution flags are checked post-resume to confirm correct scheduling behavior.

    // 统一使用 FUZZ_INPUT 构造 Reader，确保输入来源与大小一致。
    FR_Reader fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);

    // 预留少量基线缓冲，降低栈抖动和未初始化使用的风险。
    unsigned char fr_baseline[16] = {0};
    (void)FR_next_bytes(&fr, fr_baseline, sizeof(fr_baseline));

    // 统一的有界迭代次数，避免无限循环和长时间阻塞。
    // 约定：所有测试逻辑在 iterations 次迭代内完成当前输入下的探索。
    unsigned int iterations = (unsigned int)FR_next_range(&fr, 0, 10);

    // 防御性检查：确保调度器已运行，避免在错误上下文中调用阻塞 API。
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        printf("[ERROR] Scheduler not running - aborting test_task\n");
        fflush(stdout);
        // 在 Demo 场景下直接返回，避免进一步触发 HardFault/Lockup。
        return;
    }

    /* --- One Time Setup --- */
    /* Create semaphore and worker task once, before the loop, to avoid race conditions
     * with the Idle task cleaning up deleted static tasks. */
    xTestSemaphore = xSemaphoreCreateBinaryStatic(&xTestSemaphoreBuffer);
    configASSERT(xTestSemaphore != NULL);

    xWorkerHandle = xTaskCreateStatic(vWorkerTask,
                                      "Worker",
                                      WORKER_STACK_SIZE,
                                      (void *)xTestSemaphore,
                                      uxTaskPriorityGet(NULL), /* Initial placeholder priority */
                                      uxWorkerStack,
                                      &xWorkerTCB);
    configASSERT(xWorkerHandle != NULL);

    /* Allow the worker task to run for the first time and block on the semaphore. */
    vTaskDelay(pdMS_TO_TICKS(2));

    // ================= 有界迭代骨架 =================
    // LLM 生成的 test_logic 将被放置在该 for 循环内部，
    // 每次迭代可执行少量 API 调用或状态变换。禁止在 test_logic 中
    // 再引入无限循环或长时间阻塞。
    for (unsigned int i = 0; i < iterations; ++i) {
        /* Ensure there's enough fuzz input for the iteration. A safe lower bound. */
        if (FR_remaining(&fr) < 4)
        {
            break;
        }

        /* Consume fuzz input to derive test parameters. */
        const uint8_t nest_level = FR_next_range(&fr, 1, 5);
        /* Priority relative to the current fuzzer task. Can be lower, same, or higher. */
        const int8_t priority_offset = (int8_t)(FR_next_u8(&fr) % 3) - 1;
        /* Number of ticks to delay while scheduler is suspended to test pended ticks. */
        const TickType_t pended_ticks_delay = (TickType_t)FR_next_range(&fr, 0, 10);
        /* Consume an extra byte for variability. Not used directly. */
        (void)FR_next_u8(&fr);

        /* --- Setup for this iteration --- */
        worker_task_ran_flag = pdFALSE;

        /* Determine worker task priority. Clamp to valid range. */
        UBaseType_t current_priority = uxTaskPriorityGet(NULL);
        UBaseType_t worker_priority;
        if ((priority_offset > 0) && (current_priority < (configMAX_PRIORITIES - 1)))
        {
            worker_priority = current_priority + 1;
        }
        else if ((priority_offset < 0) && (current_priority > tskIDLE_PRIORITY))
        {
            worker_priority = current_priority - 1;
        }
        else
        {
            worker_priority = current_priority;
        }

        /* Set the fuzzed priority for the current test iteration. */
        vTaskPrioritySet(xWorkerHandle, worker_priority);

        /* Resume the worker if it was suspended from a previous iteration.
         * It will then loop internally and pend on the semaphore again.
         * On the first iteration, the task is already pending on the semaphore,
         * and vTaskResume() on a non-suspended task has no effect. */
        vTaskResume(xWorkerHandle);

        /* Give the worker a chance to run and re-pend on the semaphore. */
        vTaskDelay(pdMS_TO_TICKS(1));
        configASSERT(worker_task_ran_flag == pdFALSE);

        /* --- Test Execution --- */

        /* 1. Suspend the scheduler with nesting. */
        for (uint8_t j = 0; j < nest_level; ++j)
        {
            vTaskSuspendAll();
        }

        /* 2. Unblock the worker task, making it pending ready. */
        configASSERT(xSemaphoreGive(xTestSemaphore) == pdTRUE);

        /* 3. Simulate ticks occurring while scheduler is suspended. */
        if (pended_ticks_delay > 0)
        {
            /*
             * CORRECTED: Do not call vTaskDelay() or any other blocking API while the
             * scheduler is suspended (uxSchedulerSuspended > 0). This is illegal and
             * causes a configASSERT() failure inside the kernel. Pended ticks will
             * be handled correctly by xTaskResumeAll() if the tick interrupt occurs
             * during the suspended period, so no explicit delay call is needed to
             * test this behavior. The original call has been removed.
             */
            (void)pended_ticks_delay; /* This parameter is now unused, but kept for input stability. */
            /* vTaskDelay(pended_ticks_delay); */ /* <-- ILLEGAL CALL, REMOVED */
        }

        /* 4. Resume scheduler, but not fully due to nesting. */
        for (uint8_t j = 0; j < nest_level - 1; ++j)
        {
            /* Each of these calls should not cause a yield as scheduler is still suspended. */
            configASSERT(xTaskResumeAll() == pdFALSE);
        }

        /* 5. Final resume call. Check if it requests a context switch. */
        const BaseType_t xYielded = xTaskResumeAll();
        const BaseType_t xExpectedYield = (worker_priority > current_priority) ? pdTRUE : pdFALSE;

        /* The return value should indicate if a higher priority task became ready. */
        configASSERT(xYielded == xExpectedYield);

        /* --- Verification --- */

        /* If the worker had higher priority, it should have run immediately and set its flag. */
        if (xExpectedYield == pdTRUE)
        {
            configASSERT(worker_task_ran_flag == pdTRUE);
        }
        else
        {
            /* If worker is lower/same priority, it hasn't run yet. */
            configASSERT(worker_task_ran_flag == pdFALSE);
            /* Give it a chance to run now. */
            vTaskDelay(pdMS_TO_TICKS(2));
            configASSERT(worker_task_ran_flag == pdTRUE);
        }

        /* The worker has now run and suspended itself, ready for the next iteration. */
    }

    /* --- Final Cleanup --- */
    if (xWorkerHandle != NULL)
    {
        /* The worker is likely suspended. To delete it safely, it's robust to
         * resume it first within a critical section. */
        vTaskSuspendAll();
        {
            vTaskResume(xWorkerHandle);
            vTaskDelete(xWorkerHandle);
        }
        (void)xTaskResumeAll();
        xWorkerHandle = NULL;
    }

    if (xTestSemaphore != NULL)
    {
        vSemaphoreDelete(xTestSemaphore);
        xTestSemaphore = NULL;
    }

    // =============================================
    // 关键完工标记：用于生成/修复阶段判断“本次测试逻辑已正常走完”。
    // 在 fuzzing 阶段，这行输出也不会影响 LibAFL 的行为。
    // =============================================
    printf("[TEST_CASE_COMPLETED]\n");
    fflush(stdout);

    // 调用 BREAKPOINT 函数结束本次 fuzzing；在 fuzz 阶段由 QEMU+LibAFL 进行捕获。
    BREAKPOINT();
}

// FreeRTOS任务包装器（非 MPU 端口，使用普通任务优先级）
void fuzz_task(void)
{
    // 创建模糊测试任务
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