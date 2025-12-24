/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: vTaskStartScheduler_StaticInit_Fuzz
 * 生成时间: 2025-12-23 19:42:16
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
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

// LibAFL集成必需的全局变量
#define MAX_FUZZ_INPUT_SIZE 1024
__attribute__((used, visibility("default"))) unsigned char FUZZ_INPUT[MAX_FUZZ_INPUT_SIZE] = {
    0x46, 0x55, 0x5a, 0x5a, 0x01, 0x23, 0x45, 0x67,
    0x89, 0xab, 0xcd, 0xef, 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc,
    0xdd, 0xee, 0xff, 0x00, 0x13, 0x37, 0x42, 0x24
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
static inline uint32_t FR_next_u32(FR_Reader* r) {
    uint32_t res = 0;
    if (r->off + 4 <= r->size) {
        res = (uint32_t)r->data[r->off] | ((uint32_t)r->data[r->off+1] << 8) | ((uint32_t)r->data[r->off+2] << 16) | ((uint32_t)r->data[r->off+3] << 24);
        r->off += 4;
    }
    return res;
}
static inline uint32_t FR_next_range(FR_Reader* r, uint32_t min_v, uint32_t max_v) {
    if (max_v <= min_v) return min_v;
    return min_v + (FR_next_u32(r) % (max_v - min_v + 1u));
}
static inline size_t FR_next_bytes(FR_Reader* r, unsigned char* out, size_t n) {
    size_t rem = FR_remaining(r); if (n > rem) n = rem; if (n) { memcpy(out, r->data + r->off, n); r->off += n; } return n;
}

// 静态资源声明
static StaticTask_t xFuzzTCB;
static StackType_t xFuzzStack[configMINIMAL_STACK_SIZE];

// 注：vApplicationGetIdleTaskMemory 和 vApplicationGetTimerTaskMemory 
// 已由 main.c 提供，为避免 [multiple definition] 错误，本 harness 不再重复定义。

static void vDummyFuzzTask(void *pv) {
    (void)pv;
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ====================================================================
// 主测试函数
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void *pvParameters)
{
    (void)pvParameters;
    FR_Reader fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);

    unsigned char fr_baseline[16] = {0};
    (void)FR_next_bytes(&fr, fr_baseline, sizeof(fr_baseline));

    unsigned int iterations = (unsigned int)FR_next_range(&fr, 1, 10);

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return;
    }

    for (unsigned int i = 0; i < iterations; ++i) {
        if (FR_remaining(&fr) < 16) break;

        /* Fuzzing task creation logic via static APIs. */
        /* This exercises the core of task_management similarly to how prvCreateIdleTasks does. */
        UBaseType_t uxPriority = (UBaseType_t)FR_next_range(&fr, 1, configMAX_PRIORITIES - 1);
        
        TaskHandle_t xHandle = xTaskCreateStatic(
            vDummyFuzzTask, 
            "Fuzz", 
            configMINIMAL_STACK_SIZE, 
            NULL, 
            uxPriority, 
            xFuzzStack, 
            &xFuzzTCB
        );

        if (xHandle != NULL) {
            /* Fuzz priority manipulation */
            UBaseType_t uxNewPriority = (UBaseType_t)FR_next_range(&fr, 1, configMAX_PRIORITIES - 1);
            vTaskPrioritySet(xHandle, uxNewPriority);
    
            /* Exercise scheduler lock/unlock mechanism */
            vTaskSuspendAll();
            (void)uxTaskGetNumberOfTasks();
            xTaskResumeAll();

            /* Cleanup to allow reuse of static stack/TCB in next iteration */
            vTaskDelete(xHandle);
        }
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
        for (;;) {}
    }

    // 调度器可能由 main() 启动，此处逻辑保持模板一致性
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        vTaskStartScheduler();
    }

    for (;;) {}
}