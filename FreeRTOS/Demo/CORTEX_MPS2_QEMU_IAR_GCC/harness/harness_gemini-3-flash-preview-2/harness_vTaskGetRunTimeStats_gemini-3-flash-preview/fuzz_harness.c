/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: vTaskGetRunTimeStats_Fuzz
 * 生成时间: 2025-12-26 15:06:03
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
#include <stdbool.h>
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
static inline uint32_t FR_next_u32(FR_Reader* r) {
    uint32_t b1 = FR_next_u8(r);
    uint32_t b2 = FR_next_u8(r);
    uint32_t b3 = FR_next_u8(r);
    uint32_t b4 = FR_next_u8(r);
    return (b4 << 24) | (b3 << 16) | (b2 << 8) | b1;
}
static inline uint32_t FR_next_range(FR_Reader* r, uint32_t min_v, uint32_t max_v) {
    if (max_v <= min_v) return min_v;
    uint32_t span = max_v - min_v + 1u;
    return min_v + (FR_next_u32(r) % span);
}
static inline size_t FR_next_bytes(FR_Reader* r, unsigned char* out, size_t n) {
    size_t rem = FR_remaining(r); if (n > rem) n = rem; if (n) { memcpy(out, r->data + r->off, n); r->off += n; } return n;
}

#define MAX_DUMMY_TASKS 3
#define STATS_BUFFER_SIZE 1024

static StaticTask_t xDummyTCBs[MAX_DUMMY_TASKS];
static StackType_t xDummyStacks[MAX_DUMMY_TASKS][configMINIMAL_STACK_SIZE];
static TaskHandle_t xDummyHandles[MAX_DUMMY_TASKS];
static bool xDummySuspended[MAX_DUMMY_TASKS];
static char pcStatsBuffer[STATS_BUFFER_SIZE];

#ifndef configSTATS_BUFFER_MAX_LENGTH
    #define configSTATS_BUFFER_MAX_LENGTH STATS_BUFFER_SIZE
#endif

static void vFuzzDummyTask(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

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
        if (FR_remaining(&fr) < 8) break;

        uint8_t ucAction = FR_next_u8(&fr);
        uint8_t ucTaskIdx = FR_next_range(&fr, 0, MAX_DUMMY_TASKS - 1);
        uint8_t ucPriority = (uint8_t)FR_next_range(&fr, tskIDLE_PRIORITY, configMAX_PRIORITIES - 1);
        uint32_t ulDelay = FR_next_range(&fr, 0, 5);

        switch (ucAction % 5) {
            case 0: /* Create Task */
                if (xDummyHandles[ucTaskIdx] == NULL) {
                    xDummyHandles[ucTaskIdx] = xTaskCreateStatic(
                        vFuzzDummyTask, "FuzzD", configMINIMAL_STACK_SIZE, NULL, 
                        (UBaseType_t)(ucPriority | portPRIVILEGE_BIT), 
                        xDummyStacks[ucTaskIdx], &xDummyTCBs[ucTaskIdx]);
                    xDummySuspended[ucTaskIdx] = false;
                }
                break;

            case 1: /* Delete Task */
                if (xDummyHandles[ucTaskIdx] != NULL) {
                    vTaskDelete(xDummyHandles[ucTaskIdx]);
                    xDummyHandles[ucTaskIdx] = NULL;
                }
                break;

            case 2: /* Suspend/Resume Task */
                if (xDummyHandles[ucTaskIdx] != NULL) {
                    if (!xDummySuspended[ucTaskIdx]) {
                        vTaskSuspend(xDummyHandles[ucTaskIdx]);
                        xDummySuspended[ucTaskIdx] = true;
                    } else {
                        vTaskResume(xDummyHandles[ucTaskIdx]);
                        xDummySuspended[ucTaskIdx] = false;
                    }
                }
                break;

            case 3: /* Change Priority */
                if (xDummyHandles[ucTaskIdx] != NULL) {
                    vTaskPrioritySet(xDummyHandles[ucTaskIdx], (UBaseType_t)ucPriority);
                }
                break;

            case 4: /* Delay */
                vTaskDelay(pdMS_TO_TICKS(ulDelay));
                break;
        }

        memset(pcStatsBuffer, 0, STATS_BUFFER_SIZE);
        
        /* Fix: Guard vTaskGetRunTimeStats with kernel config macros to avoid linker errors. 
           Fallback to vTaskList if available, as it also exercises task list traversal. */
        #if ( configGENERATE_RUN_TIME_STATS == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 )
            vTaskGetRunTimeStats(pcStatsBuffer);
        #elif ( configUSE_TRACE_FACILITY == 1 ) && ( configUSE_STATS_FORMATTING_FUNCTIONS > 0 )
            vTaskList(pcStatsBuffer);
        #else
            (void)pcStatsBuffer;
        #endif

        if (uxTaskGetNumberOfTasks() > 0) {
            configASSERT(pcStatsBuffer[STATS_BUFFER_SIZE - 1] == '\0');
        }
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
