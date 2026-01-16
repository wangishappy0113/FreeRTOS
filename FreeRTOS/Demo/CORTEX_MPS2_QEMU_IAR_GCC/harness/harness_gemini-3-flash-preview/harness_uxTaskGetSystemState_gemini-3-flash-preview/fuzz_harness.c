/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: fuzz_task_system_state_and_suspension
 * 生成时间: 2025-12-24 13:47:04
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: fuzz_task_system_state_and_suspension
 * 描述: Fuzzes uxTaskGetSystemState and vTaskSuspendAll by exercising scheduler suspension nesting, varying task status array sizes, and toggling runtime stats collection. Uses static task creation and validates the consistency of reported task counts.
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
#include <stdbool.h>

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

// LLM可在此添加非冲突的辅助函数
static TaskStatus_t xStatusArray[12];
static StaticTask_t xDummyTCB[2];
static StackType_t uxDummyStack[2][configMINIMAL_STACK_SIZE];
static TaskHandle_t xDummyHandles[2] = { NULL, NULL };
static bool bInitialized = false;

static void vDummyFuzzTask(void *pv) {
    (void)pv;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 主测试函数
void __attribute__((used, visibility("default"))) test_task(void *pvParameters)
{
    (void)pvParameters;
    FR_Reader fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);

    // 预留少量基线缓冲
    unsigned char fr_baseline[16] = {0};
    (void)FR_next_bytes(&fr, fr_baseline, sizeof(fr_baseline));

    unsigned int iterations = (unsigned int)FR_next_range(&fr, 0, 10);

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return;
    }

    for (unsigned int i = 0; i < iterations; ++i) {
        if (!bInitialized) {
            /* Reset reader for persistent initialization if needed */
            xDummyHandles[0] = xTaskCreateStatic(vDummyFuzzTask, "FuzzD0", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1) | portPRIVILEGE_BIT, uxDummyStack[0], &xDummyTCB[0]);
            xDummyHandles[1] = xTaskCreateStatic(vDummyFuzzTask, "FuzzD1", configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1) | portPRIVILEGE_BIT, uxDummyStack[1], &xDummyTCB[1]);
            bInitialized = true;
        }

        if (FR_remaining(&fr) < 12) {
            break;
        }

        uint8_t ucNestCount = (uint8_t)FR_next_range(&fr, 0, 4);
        uint8_t ucPassedSize = (uint8_t)FR_next_range(&fr, 0, 12);
        uint8_t ucUseRuntime = FR_next_u8(&fr) & 0x01;
        uint8_t ucAction0 = FR_next_u8(&fr) % 3;
        uint8_t ucAction1 = FR_next_u8(&fr) % 3;

        if (xDummyHandles[0] != NULL) {
            if (ucAction0 == 1) vTaskSuspend(xDummyHandles[0]);
            else if (ucAction0 == 2) vTaskResume(xDummyHandles[0]);
        }
        if (xDummyHandles[1] != NULL) {
            if (ucAction1 == 1) vTaskSuspend(xDummyHandles[1]);
            else if (ucAction1 == 2) vTaskResume(xDummyHandles[1]);
        }

        /* Exercise scheduler suspension nesting */
        for (uint8_t j = 0; j < ucNestCount; j++) {
            vTaskSuspendAll();
        }

        configRUN_TIME_COUNTER_TYPE ulTotalRunTime = 0;
        UBaseType_t uxReturnedTaskCount = 0;

        /* Target API call */
        uxReturnedTaskCount = uxTaskGetSystemState(xStatusArray, (UBaseType_t)ucPassedSize, ucUseRuntime ? &ulTotalRunTime : NULL);

        /* Verify result consistency */
        UBaseType_t uxActualTaskCount = uxTaskGetNumberOfTasks();
        if ((UBaseType_t)ucPassedSize >= uxActualTaskCount) {
            configASSERT(uxReturnedTaskCount == uxActualTaskCount || uxReturnedTaskCount == 0);
        } else {
            configASSERT(uxReturnedTaskCount == 0);
        }

        /* Clean up nested suspension */
        for (uint8_t j = 0; j < ucNestCount; j++) {
            (void)xTaskResumeAll();
        }

        vTaskDelay(pdMS_TO_TICKS(1));
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