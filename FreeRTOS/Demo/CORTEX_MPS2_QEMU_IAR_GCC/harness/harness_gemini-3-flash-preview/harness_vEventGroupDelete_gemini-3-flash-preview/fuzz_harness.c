/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: vEventGroupDelete_WorkerUnblock
 * 生成时间: 2025-12-23 21:15:13
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: event_groups
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"
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
static inline uint32_t FR_next_range(FR_Reader* r, uint32_t min_v, uint32_t max_v) {
    if (max_v <= min_v) return min_v;
    uint32_t span = max_v - min_v + 1u;
    return min_v + (FR_next_u32(r) % span);
}
static inline size_t FR_next_bytes(FR_Reader* r, unsigned char* out, size_t n) {
    size_t rem = FR_remaining(r); if (n > rem) n = rem; if (n) { memcpy(out, r->data + r->off, n); r->off += n; } return n;
}

static StaticEventGroup_t xStaticEventGroup;
static EventGroupHandle_t xEventGroupHandle;

#define MAX_WORKER_TASKS 2
static StackType_t uxWorkerStacks[MAX_WORKER_TASKS][configMINIMAL_STACK_SIZE];
static StaticTask_t xWorkerTaskBuffers[MAX_WORKER_TASKS];
static TaskHandle_t xWorkerTaskHandles[MAX_WORKER_TASKS];

typedef struct {
    EventGroupHandle_t xGroup;
    EventBits_t uxBitsToWait;
    BaseType_t xWaitForAll;
    volatile BaseType_t xResultCorrect;
    volatile BaseType_t xFinished;
} WorkerParams_t;

static WorkerParams_t xWorkerParams[MAX_WORKER_TASKS];

static void vWorkerTask(void *pvParameters) {
    WorkerParams_t *pxParams = (WorkerParams_t *)pvParameters;
    EventBits_t uxReturn = xEventGroupWaitBits(
        pxParams->xGroup,
        pxParams->uxBitsToWait,
        pdTRUE,
        pxParams->xWaitForAll,
        portMAX_DELAY
    );

    if (uxReturn == 0) {
        pxParams->xResultCorrect = pdTRUE;
    } else {
        pxParams->xResultCorrect = pdFALSE;
    }
    pxParams->xFinished = pdTRUE;
    vTaskDelete(NULL);
}

void __attribute__((used, visibility("default"))) test_task(void *pvParameters)
{
    (void)pvParameters;
    FR_Reader fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);
    unsigned char fr_baseline[16] = {0};
    (void)FR_next_bytes(&fr, fr_baseline, sizeof(fr_baseline));

    unsigned int iterations = (unsigned int)FR_next_range(&fr, 0, 10);

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return;
    }

    for (unsigned int i = 0; i < iterations; ++i) {
        if (FR_remaining(&fr) < 16) {
            break;
        }

        xEventGroupHandle = xEventGroupCreateStatic(&xStaticEventGroup);
        if (xEventGroupHandle == NULL) {
            return;
        }

        uint8_t ucNumTasks = (uint8_t)FR_next_range(&fr, 1, MAX_WORKER_TASKS);
        EventBits_t uxWaitBits = FR_next_u32(&fr) & 0x00FFFFFF;
        BaseType_t xWaitAll = (FR_next_u8(&fr) % 2 == 0) ? pdFALSE : pdTRUE;
        TickType_t xSettleDelay = (TickType_t)FR_next_range(&fr, 1, 10);

        for (uint8_t j = 0; j < ucNumTasks; j++) {
            xWorkerParams[j].xGroup = xEventGroupHandle;
            xWorkerParams[j].uxBitsToWait = (uxWaitBits >> j) | 0x01;
            xWorkerParams[j].xWaitForAll = xWaitAll;
            xWorkerParams[j].xResultCorrect = pdFALSE;
            xWorkerParams[j].xFinished = pdFALSE;

            xWorkerTaskHandles[j] = xTaskCreateStatic(
                vWorkerTask,
                "EventWrk",
                configMINIMAL_STACK_SIZE,
                (void *)&xWorkerParams[j],
                ((FUZZ_TASK_PRIORITY - 1) | portPRIVILEGE_BIT),
                uxWorkerStacks[j],
                &xWorkerTaskBuffers[j]
            );
            configASSERT(xWorkerTaskHandles[j] != NULL);
        }

        vTaskDelay(xSettleDelay);
        vEventGroupDelete(xEventGroupHandle);
        vTaskDelay(xSettleDelay + 5);

        for (uint8_t j = 0; j < ucNumTasks; j++) {
            if (xWorkerParams[j].xFinished == pdTRUE) {
                configASSERT(xWorkerParams[j].xResultCorrect == pdTRUE);
            } else {
                vTaskDelete(xWorkerTaskHandles[j]);
            }
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

    if (xHandle == NULL) {
        configASSERT(0);
        for (;;) {}
    }

    vTaskStartScheduler();
    for (;;) {}
}