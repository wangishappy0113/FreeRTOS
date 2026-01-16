/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: xTaskCatchUpTicks_TaskUnblock_Fuzz_Fixed
 * 修复说明: 使用 xTaskCreate 替代 xTaskCreateStatic 以修复循环中的 Use-After-Free/TCB Corruption
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// ====================================================================
// LibAFL集成 - Crash Input 植入
// ====================================================================
#define MAX_FUZZ_INPUT_SIZE 1024
__attribute__((used, visibility("default"))) unsigned char FUZZ_INPUT[MAX_FUZZ_INPUT_SIZE] = {
    // 植入的 Crash 样本 (来自 Hexdump)
    0x64, 0xff, 0xff, 0x0e, 0x0e, 0xff, 0x77, 0x88, 0x30, 0x55, 0x55, 0x55, 0x55, 0x6e, 0x6e, 0x00,
    0x1a, 0x00, 0xfe, 0x75, 0x80, 0x00, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x95, 0x95, 0x95, 0x95,
    0x95, 0x0c, 0xf7, 0x0d, 0x0d, 0x0d, 0x01, 0x40, 0x0e, 0x94, 0x46, 0x55, 0x5a, 0xba, 0xf7, 0xfb,
    0x7f, 0xf0, 0x30, 0x00, 0x67, 0x89, 0xab, 0x1e, 0x1e, 0x28, 0x1e, 0x1e, 0x1e, 0x1e, 0xfe, 0x22,
    0x00, 0x46, 0xab, 0x5a, 0xba, 0xa2, 0xff, 0xa2, 0xc3, 0xa2, 0xa2, 0xa2, 0xa2, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x23, 0x55, 0x55, 0x55, 0xfe, 0x55, 0xff, 0xff, 0xff, 0x01, 0x55, 0x55, 0x55,
    0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x11, 0x22, 0x46, 0xab, 0x5a, 0xba, 0x00, 0x00,
    // 剩余填充 0
    0x00
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
#ifdef portPRIVILEGE_BIT
#define FUZZ_TASK_PRIORITY ((tskIDLE_PRIORITY + 1) | portPRIVILEGE_BIT)
#else
#define FUZZ_TASK_PRIORITY (tskIDLE_PRIORITY + 1)
#endif

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

// --------------------------------------------------------
// [FIX] 移除了全局静态 worker buffer，改用动态分配
// --------------------------------------------------------
static StaticSemaphore_t xSemBuffer;
static SemaphoreHandle_t xDoneSem;
static FR_Reader fr_inst;
static bool fr_is_init = false;

static void vWorkerTask(void *pvParameters) {
    TickType_t xDelay = (TickType_t)(uintptr_t)pvParameters;
    if (xDelay > 0) {
        vTaskDelay(xDelay);
    }
    xSemaphoreGive(xDoneSem);
    
    // Worker 任务自我删除
    // 注意：vTaskDelete(NULL) 会将内存释放任务交给 Idle Task
    vTaskDelete(NULL); 
}

// ====================================================================
// 主测试函数
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void *pvParameters)
{
    (void)pvParameters;
    FR_Reader fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);

    // 预留缓冲
    unsigned char fr_baseline[16] = {0};
    (void)FR_next_bytes(&fr, fr_baseline, sizeof(fr_baseline));

    unsigned int iterations = (unsigned int)FR_next_range(&fr, 0, 10);

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        printf("[ERROR] Scheduler not running\n");
        return;
    }

    // ================= 有界迭代 =================
    for (unsigned int i = 0; i < iterations; ++i) {
        if (!fr_is_init) {
            fr_inst = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);
            xDoneSem = xSemaphoreCreateBinaryStatic(&xSemBuffer);
            fr_is_init = true;
        }

        if (FR_remaining(&fr_inst) < 12) {
            return;
        }

        TickType_t xCatchUpTicks = (TickType_t)FR_next_u32(&fr_inst) % 128;
        TickType_t xBlockTime = (TickType_t)FR_next_u32(&fr_inst) % 128;
        UBaseType_t uxWorkerPriority = (UBaseType_t)FR_next_range(&fr_inst, tskIDLE_PRIORITY + 1, configMAX_PRIORITIES - 1);

        // [FIX] 这里的关键修改：使用 xTaskCreate 而不是 xTaskCreateStatic
        // 这确保每次迭代都有独立的内存，不会干扰正在等待 Idle Task 清理的旧任务 TCB
        TaskHandle_t xWorkerHandle = NULL;
        BaseType_t xRet = xTaskCreate(
            vWorkerTask,
            "Worker",
            configMINIMAL_STACK_SIZE,
            (void *)(uintptr_t)xBlockTime,
            uxWorkerPriority | portPRIVILEGE_BIT,
            &xWorkerHandle // 句柄传出
        );

        if (xRet == pdPASS && xWorkerHandle != NULL) {
            /* Ensure the semaphore is empty before starting. */
            xSemaphoreTake(xDoneSem, 0);

            /* Give the worker task a short window to run and enter the blocked state. */
            vTaskDelay(2);

            /* Call the target API. */
            BaseType_t xYieldOccurred = xTaskCatchUpTicks(xCatchUpTicks);
            (void)xYieldOccurred;

            /* Check if the worker task finished within a reasonable time. */
            if (xSemaphoreTake(xDoneSem, 10) == pdFALSE) {
                /* If the task is still active, clean it up. */
                vTaskDelete(xWorkerHandle);
                
                // 给 Idle Task 一点时间来回收内存，防止堆耗尽（虽然在短循环中很少见）
                vTaskDelay(2);
            } else {
                // 任务已自行结束 (vWorkerTask 中调用了 vTaskDelete)
                // 同样给 Idle Task 一点时间回收
                vTaskDelay(1);
            }
        }
    }

    printf("[TEST_CASE_COMPLETED]\n");
    fflush(stdout);

    BREAKPOINT();
}

// FreeRTOS任务包装器
void fuzz_task(void)
{
    // Fuzz Task 本身只创建一次，可以使用 Static 以节省堆内存
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
        for (;;) {}
    }

    vTaskStartScheduler();

    for (;;) {}
}