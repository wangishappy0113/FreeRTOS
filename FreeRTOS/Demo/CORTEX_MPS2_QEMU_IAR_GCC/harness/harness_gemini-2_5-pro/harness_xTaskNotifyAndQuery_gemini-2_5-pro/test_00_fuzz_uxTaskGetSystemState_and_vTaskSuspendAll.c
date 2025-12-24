/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: fuzz_uxTaskGetSystemState_and_vTaskSuspendAll
 * 生成时间: 2025-12-23 15:19:52
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: fuzz_uxTaskGetSystemState_and_vTaskSuspendAll
 * 描述: Fuzzes uxTaskGetSystemState by creating multiple tasks and putting them into various states (Ready, Blocked, Suspended). It calls uxTaskGetSystemState with fuzzed parameters for the array size and runtime pointer. It also fuzzes the nesting level of vTaskSuspendAll/xTaskResumeAll calls that wrap the main API call. The test validates the number of populated entries and performs basic sanity checks on the returned task status information.
 * 
 * 生成上下文:
 * 检测到的API函数: uxTaskGetSystemState, CMSDK_SYSCON_EMICTRL_RCYC_Msk          , enabled , Pointer , CMSDK_SYSCON_PMUCTRL_EN_Msk            , MPU_CTRL_ENABLE_Msk                , CMSDK_SYSCON_EMICTRL_SIZE_Msk          , __RRX, xTimer0Handler, __LDAEXB
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
/* Max number of test tasks to create */
#define MAX_VICTIM_TASKS 4

/* Static storage for victim tasks */
static StackType_t ucVictimTaskStacks[MAX_VICTIM_TASKS][configMINIMAL_STACK_SIZE];
static StaticTask_t xVictimTaskTCBs[MAX_VICTIM_TASKS];
static TaskHandle_t xVictimTaskHandles[MAX_VICTIM_TASKS];

/* Static storage for uxTaskGetSystemState. Size needs to account for all possible tasks. */
#define MAX_SYSTEM_TASKS (MAX_VICTIM_TASKS + 4) /* victims + fuzz_task + idle + timer + etc */
static TaskStatus_t xTaskStatusArray[MAX_SYSTEM_TASKS];
static configRUN_TIME_COUNTER_TYPE ulTotalRunTime;

/* 
A simple victim task that can be put into a Blocked state by delaying.
   It loops infinitely, waiting to be deleted by the main test task to ensure deterministic cleanup.
*/
static void vVictimTask(void *pvParameters)
{
    TickType_t xDelayTicks = (TickType_t)(uintptr_t)pvParameters;
    
    if (xDelayTicks > 0)
    {
        vTaskDelay(xDelayTicks);
    }

    /* After any initial delay, loop indefinitely until deleted by the test runner. */
    for(;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void *pvParameters)
{
    (void) pvParameters; /* Avoid unused parameter warning. */

    // 测试用例: fuzz_uxTaskGetSystemState_and_vTaskSuspendAll
    // API类别: task_management
    // 描述: Fuzzes uxTaskGetSystemState by creating multiple tasks and putting them into various states (Ready, Blocked, Suspended). It calls uxTaskGetSystemState with fuzzed parameters for the array size and runtime pointer. It also fuzzes the nesting level of vTaskSuspendAll/xTaskResumeAll calls that wrap the main API call. The test validates the number of populated entries and performs basic sanity checks on the returned task status information.

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

    // ================= 有界迭代骨架 =================
    // LLM 生成的 test_logic 将被放置在该 for 循环内部，
    // 每次迭代可执行少量 API 调用或状态变换。禁止在 test_logic 中
    // 再引入无限循环或长时间阻塞。
    for (unsigned int i = 0; i < iterations; ++i) {
            /* Ensure there is enough fuzz input for a meaningful iteration */
            if (FR_remaining(&fr) < 16)
            {
                break;
            }

            /* Fuzz-driven parameters for this iteration */
            UBaseType_t num_victims = FR_next_range(&fr, 1, MAX_VICTIM_TASKS);
            UBaseType_t suspend_idx = FR_next_u8(&fr) % num_victims;
            TickType_t delay_ticks_fuzz = FR_next_range(&fr, 10, 50);
            uint8_t array_size_mode = FR_next_u8(&fr) % 4; /* 0: zero, 1: too small, 2: exact, 3: too large */
            uint8_t use_null_runtime_ptr = FR_next_u8(&fr) & 1;
            UBaseType_t nest_level = FR_next_range(&fr, 1, 4);

            /* Create victim tasks for this iteration */
            for (UBaseType_t j = 0; j < num_victims; j++)
            {
                /* Parameter for vVictimTask is the delay in ticks. A task to be suspended shouldn't block. */
                void *pvTaskParam = (void *)(uintptr_t)(j == suspend_idx ? 0 : delay_ticks_fuzz);
                UBaseType_t priority = (tskIDLE_PRIORITY + 1 + j) % configMAX_PRIORITIES;
                if (priority == FUZZ_TASK_PRIORITY) { 
                    priority = (priority + 1) % configMAX_PRIORITIES;
                    if (priority == tskIDLE_PRIORITY) { priority++; } /* Ensure priority > idle */
                }

                xVictimTaskHandles[j] = xTaskCreateStatic(vVictimTask,
                                                          "Victim",
                                                          configMINIMAL_STACK_SIZE,
                                                          pvTaskParam,
                                                          priority,
                                                          ucVictimTaskStacks[j],
                                                          &xVictimTaskTCBs[j]);
                configASSERT(xVictimTaskHandles[j] != NULL);
            }

            /* Let the scheduler run to allow tasks to enter their states */
            vTaskDelay(pdMS_TO_TICKS(2));

            /* Suspend one of the victim tasks to test the eSuspended state capture */
            vTaskSuspend(xVictimTaskHandles[suspend_idx]);

            /* Give the system a moment to settle after suspension */
            vTaskDelay(pdMS_TO_TICKS(2));

            /* Determine parameters for uxTaskGetSystemState based on fuzz input */
            UBaseType_t current_task_count = uxTaskGetNumberOfTasks();
            configASSERT(current_task_count <= MAX_SYSTEM_TASKS);
            UBaseType_t uxArraySize = 0;
            switch (array_size_mode)
            {
                case 0: /* Zero size */
                    uxArraySize = 0;
                    break;
                case 1: /* Too small */
                    if (current_task_count > 1) { uxArraySize = FR_next_range(&fr, 1, current_task_count - 1); } else { uxArraySize = 0; }
                    break;
                case 2: /* Exact size */
                    uxArraySize = current_task_count;
                    break;
                case 3: /* Too large */
                    uxArraySize = current_task_count + FR_next_range(&fr, 1, 5);
                    break;
            }

            configRUN_TIME_COUNTER_TYPE *pulTotalRunTimePtr = use_null_runtime_ptr ? NULL : &ulTotalRunTime;
    
            /* Test vTaskSuspendAll nesting */
            for (UBaseType_t j = 0; j < nest_level; j++)
            {
                vTaskSuspendAll();
            }

            /* Call the target API while scheduler is suspended */
            UBaseType_t populated_entries = uxTaskGetSystemState(xTaskStatusArray, uxArraySize, pulTotalRunTimePtr);

            /* Resume the scheduler */
            for (UBaseType_t j = 0; j < nest_level; j++)
            {
                (void)xTaskResumeAll();
            }

            /* --- Assertions --- */
            if (uxArraySize < current_task_count)
            {
                configASSERT(populated_entries == 0);
            }
            else
            {
                configASSERT(populated_entries == current_task_count);
        
                UBaseType_t suspended_found = 0;
                for (UBaseType_t j = 0; j < populated_entries; j++)
                {
                    TaskStatus_t *status = &xTaskStatusArray[j];
                    configASSERT(status->uxCurrentPriority < configMAX_PRIORITIES);
                    configASSERT(status->eCurrentState <= eDeleted);

                    if (status->xHandle == xVictimTaskHandles[suspend_idx])
                    {
                        configASSERT(status->eCurrentState == eSuspended);
                        suspended_found = 1;
                    }
                }
                configASSERT(suspended_found == 1);
            }
    
            if (!use_null_runtime_ptr)
            {
                /* The value of ulTotalRunTime is not predictable, but it should have been written to.
                   The original check for >= 0 was redundant for an unsigned type and has been removed. */
            }

            /* --- Cleanup for next iteration --- */
            for (UBaseType_t j = 0; j < num_victims; j++)
            {
                if (xVictimTaskHandles[j] != NULL)
                {
                    /* Resume task in case it was suspended, then delete it. It is safe to call vTaskResume on a non-suspended task. */
                    vTaskResume(xVictimTaskHandles[j]);
                    vTaskDelete(xVictimTaskHandles[j]);
                    xVictimTaskHandles[j] = NULL;
                }
            }
            /* Give idle task time to run and free up TCBs. */
            vTaskDelay(pdMS_TO_TICKS(5));
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