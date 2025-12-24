/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: xTaskGetHandle_fuzz_basic_lookup_and_state_transitions
 * 生成时间: 2025-12-18 21:34:03
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: xTaskGetHandle_fuzz_basic_lookup_and_state_transitions
 * 描述: Fuzzes xTaskGetHandle by creating multiple static tasks with fuzz-derived names and priorities, then performing lookups using fuzz-derived query strings while exercising task state transitions (ready, delayed, suspended, and deleted) to cover different internal search lists.
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
typedef struct
{
    StaticTask_t tcb;
    StackType_t stack[128];
    TaskHandle_t handle;
    char name[configMAX_TASK_NAME_LEN];
} FuzzNamedTask;

#define FUZZ_MAX_TASKS 4

static FuzzNamedTask g_fuzzTasks[FUZZ_MAX_TASKS];
static BaseType_t g_tasksInitialized = pdFALSE;

static void fuzz_dummy_task(void *pvParameters)
{
    (void) pvParameters;
    /* Keep the task alive without blocking the scheduler for long. */
    for (;;)
    {
        vTaskDelay(1);
    }
}

static UBaseType_t fuzz_map_priority(uint8_t raw)
{
    if (configMAX_PRIORITIES <= 1U)
    {
        return 0U;
    }
    return (UBaseType_t)(raw % configMAX_PRIORITIES);
}

static void fuzz_init_tasks_once(void)
{
    if (g_tasksInitialized != pdFALSE)
    {
        return;
    }

    for (UBaseType_t i = 0; i < FUZZ_MAX_TASKS; i++)
    {
        g_fuzzTasks[i].handle = NULL;
        for (UBaseType_t j = 0; j < (UBaseType_t)sizeof(g_fuzzTasks[i].name); j++)
        {
            g_fuzzTasks[i].name[j] = '\0';
        }
    }

    g_tasksInitialized = pdTRUE;
}

static void fuzz_create_or_update_task(FuzzNamedTask *slot,
                                       const char *name,
                                       UBaseType_t priority,
                                       BaseType_t recreate)
{
    configASSERT(slot != NULL);

    if ((slot->handle != NULL) && (recreate != pdFALSE))
    {
#if (INCLUDE_vTaskDelete == 1)
        vTaskDelete(slot->handle);
#endif
        slot->handle = NULL;
    }

    if (slot->handle == NULL)
    {
        /* Ensure name is not NULL and within length constraints. */
        configASSERT(name != NULL);
        size_t len = strlen(name);
        if (len >= configMAX_TASK_NAME_LEN)
        {
            len = configMAX_TASK_NAME_LEN - 1U;
        }

        /* Copy name into persistent buffer to ensure lifetime. */
        memset(slot->name, 0, sizeof(slot->name));
        memcpy(slot->name, name, len);
        slot->name[len] = '\0';

        /* Ensure stack depth is at least configMINIMAL_STACK_SIZE to satisfy portASSERT_IF_IN_ISR checks. */
        const uint32_t stackDepth = (uint32_t)(sizeof(slot->stack) / sizeof(StackType_t));
        configASSERT(stackDepth >= configMINIMAL_STACK_SIZE);

        TaskHandle_t h = xTaskCreateStatic(
            fuzz_dummy_task,
            slot->name,
            stackDepth,
            NULL,
            priority,
            slot->stack,
            &slot->tcb);

        configASSERT(h != NULL);
        slot->handle = h;
    }
}

static void fuzz_drive_task_state(TaskHandle_t h, uint8_t actionByte)
{
    if (h == NULL)
    {
        return;
    }

    uint8_t action = (uint8_t)(actionByte % 4U);

    switch (action)
    {
        case 0U:
            /* Leave task in ready/running state. */
            break;
        case 1U:
            /* Delay the task for a short fuzz-derived period. */
            vTaskDelay(1U + (TickType_t)(actionByte & 0x07U));
            break;
        case 2U:
#if (INCLUDE_vTaskSuspend == 1)
            vTaskSuspend(h);
#endif
            break;
        case 3U:
#if (INCLUDE_vTaskDelete == 1)
            vTaskDelete(h);
#endif
            break;
        default:
            break;
    }
}


// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void *pvParameters)
{
    (void) pvParameters;

    // 统一使用 FUZZ_INPUT 构造 Reader，确保输入来源与大小一致。
    FR_Reader fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);

    // 预留少量基线缓冲，降低栈抖动和未初始化使用的风险。
    unsigned char fr_baseline[16] = {0};
    (void)FR_next_bytes(&fr, fr_baseline, sizeof(fr_baseline));

    // 统一的有界迭代次数，避免无限循环和长时间阻塞。
    unsigned int iterations = (unsigned int)FR_next_range(&fr, 0, 10);

    // 防御性检查：确保调度器已运行，避免在错误上下文中调用阻塞 API。
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        printf("[ERROR] Scheduler not running - aborting test_task\n");
        fflush(stdout);
        vTaskDelete(NULL);
    }

    // ================= 有界迭代骨架 =================
    for (unsigned int iter = 0; iter < iterations; ++iter) {
        {
            /* Ensure global task slots are initialized once. */
            fuzz_init_tasks_once();

            /* We require enough bytes to derive parameters; if not, exit loop. */
            if (FR_remaining(&fr) < 16U)
            {
                break;
            }

            /* Derive how many tasks to (re)configure this iteration. */
            uint8_t taskCountRaw = FR_next_u8(&fr);
            UBaseType_t taskCount = (UBaseType_t)(1U + (taskCountRaw % FUZZ_MAX_TASKS));

            /* For each selected task slot, derive a name, priority, and recreate flag. */
            for (UBaseType_t idx = 0; idx < taskCount; idx++)
            {
                if (FR_remaining(&fr) < 4U)
                {
                    break;
                }

                uint8_t nameLenRaw = FR_next_u8(&fr);

                /* Clamp name length to [1, configMAX_TASK_NAME_LEN - 1]. */
                size_t maxNameLen = (configMAX_TASK_NAME_LEN > 1U) ? (configMAX_TASK_NAME_LEN - 1U) : 1U;
                size_t nameLen = (size_t)(1U + (nameLenRaw % (uint8_t)maxNameLen));

                if (FR_remaining(&fr) < nameLen + 2U)
                {
                    break;
                }

                char nameBuf[configMAX_TASK_NAME_LEN];
                memset(nameBuf, 0, sizeof(nameBuf));

                /* Fill name with fuzz bytes, then ensure null termination. */
                (void)FR_next_bytes(&fr, (unsigned char *)nameBuf, nameLen);
                nameBuf[nameLen] = '\0';

                uint8_t prioRaw = FR_next_u8(&fr);
                UBaseType_t prio = fuzz_map_priority(prioRaw);

                uint8_t recreateRaw = FR_next_u8(&fr);
                BaseType_t recreate = (BaseType_t)(recreateRaw & 0x01U);

                fuzz_create_or_update_task(&g_fuzzTasks[idx], nameBuf, prio, recreate);
            }

            /* Derive a query name for xTaskGetHandle. */
            if (FR_remaining(&fr) < 2U)
            {
                break;
            }

            uint8_t queryLenRaw = FR_next_u8(&fr);

            size_t maxQueryLen = (configMAX_TASK_NAME_LEN > 1U) ? (configMAX_TASK_NAME_LEN - 1U) : 1U;
            size_t queryLen = (size_t)(1U + (queryLenRaw % (uint8_t)maxQueryLen));

            if (FR_remaining(&fr) < queryLen + 1U)
            {
                break;
            }

            char queryBuf[configMAX_TASK_NAME_LEN];
            memset(queryBuf, 0, sizeof(queryBuf));
            (void)FR_next_bytes(&fr, (unsigned char *)queryBuf, queryLen);
            queryBuf[queryLen] = '\0';

            /* Optionally drive one of the tasks into a different state before lookup. */
            uint8_t stateIndexRaw = FR_next_u8(&fr);
            UBaseType_t stateIndex = (taskCount > 0U) ? (UBaseType_t)(stateIndexRaw % taskCount) : 0U;

            uint8_t stateActionRaw = 0U;
            if (FR_remaining(&fr) > 0U)
            {
                stateActionRaw = FR_next_u8(&fr);
            }

            fuzz_drive_task_state(g_fuzzTasks[stateIndex].handle, stateActionRaw);

            /* Now perform the lookup. This will internally suspend/resume the scheduler. */
            TaskHandle_t found = xTaskGetHandle(queryBuf);
            (void)found;
        }
    }

    printf("[TEST_CASE_COMPLETED]\n");
    fflush(stdout);

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
