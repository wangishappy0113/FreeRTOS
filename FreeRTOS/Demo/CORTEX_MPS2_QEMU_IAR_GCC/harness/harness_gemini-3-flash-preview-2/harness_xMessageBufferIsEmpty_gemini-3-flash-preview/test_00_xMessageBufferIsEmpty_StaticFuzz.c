/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: xMessageBufferIsEmpty_StaticFuzz
 * 生成时间: 2025-12-26 09:59:40
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: message_buffer
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "message_buffer.h"
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
#ifdef portPRIVILEGE_BIT
#define FUZZ_TASK_PRIORITY ((tskIDLE_PRIORITY + 1) | portPRIVILEGE_BIT)
#else
#define FUZZ_TASK_PRIORITY (tskIDLE_PRIORITY + 1)
#endif

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
    uint32_t val = 0;
    for(int i=0; i<4; i++) val |= ((uint32_t)FR_next_u8(r) << (i*8));
    return val;
}
static inline uint32_t FR_next_range(FR_Reader* r, uint32_t min_v, uint32_t max_v) {
    if (max_v <= min_v) return min_v;
    uint32_t span = max_v - min_v + 1u;
    return min_v + (FR_next_u32(r) % span);
}
static inline size_t FR_next_bytes(FR_Reader* r, unsigned char* out, size_t n) {
    size_t rem = FR_remaining(r); if (n > rem) n = rem; if (n) { memcpy(out, r->data + r->off, n); r->off += n; } return n;
}

static StaticMessageBuffer_t xMsgBufStruct;
static uint8_t ucMsgBufStorage[256];
static MessageBufferHandle_t xMsgBuf = NULL;

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

    if (xMsgBuf == NULL) {
        xMsgBuf = xMessageBufferCreateStatic(sizeof(ucMsgBufStorage), ucMsgBufStorage, &xMsgBufStruct);
    }

    for (unsigned int i = 0; i < iterations; ++i) {
        if (xMsgBuf == NULL || FR_remaining(&fr) < 10) {
            break;
        }

        uint8_t action = FR_next_u8(&fr) % 4;
        if (action == 0) {
            size_t xLen = (size_t)FR_next_range(&fr, 1, 32);
            uint8_t ucTemp[32] = {0};
            size_t xSent = xMessageBufferSend(xMsgBuf, ucTemp, xLen, 0);
            if (xSent > 0) {
                configASSERT(xMessageBufferIsEmpty(xMsgBuf) == pdFALSE);
            }
        } else if (action == 1) {
            uint8_t ucRecv[64];
            xMessageBufferReceive(xMsgBuf, ucRecv, sizeof(ucRecv), 0);
        } else if (action == 2) {
            xMessageBufferReset(xMsgBuf);
            configASSERT(xMessageBufferIsEmpty(xMsgBuf) == pdTRUE);
        } else {
            BaseType_t xEmpty = xMessageBufferIsEmpty(xMsgBuf);
            BaseType_t xFull = xMessageBufferIsFull(xMsgBuf);
            if (xEmpty == pdTRUE) {
                uint8_t ucDummy;
                configASSERT(xMessageBufferReceive(xMsgBuf, &ucDummy, 1, 0) == 0);
                configASSERT(xFull == pdFALSE);
            } else if (xFull == pdTRUE) {
                configASSERT(xEmpty == pdFALSE);
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
