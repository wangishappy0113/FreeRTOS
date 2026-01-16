/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: pcQueueGetName_RegistryFuzz
 * 生成时间: 2025-12-26 17:52:23
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: queue
 */

/*
 * 测试用例详细信息:
 * 名称: pcQueueGetName_RegistryFuzz
 * 描述: Fuzzes the queue registry by creating multiple static queues, registering them with fuzzed names, and querying names. Tests registry search and name retrieval.
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
static inline size_t FR_next_string(FR_Reader* r, char* out, size_t max_len) {
    if (!out || max_len == 0) return 0;
    out[0] = '\0';
    if (max_len == 1) return 0;
    size_t max_copy = max_len - 1;
    size_t rem = FR_remaining(r);
    if (rem == 0) return 0;
    size_t span = rem < max_copy ? rem : max_copy;
    size_t len = (FR_next_u16(r) % (span + 1));
    if (len == 0) return 0;
    size_t got = FR_next_bytes(r, (unsigned char*)out, len);
    out[got] = '\0';
    return got;
}

#define MAX_QUEUES 3
#define QUEUE_LEN 5
#define ITEM_SIZE 4
#define NAME_LEN 12
static StaticQueue_t xStaticQueues[MAX_QUEUES];
static uint8_t ucQueueStorage[MAX_QUEUES][QUEUE_LEN * ITEM_SIZE];
static FR_Reader fuzz_reader;
static bool fuzz_initialized = false;

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
        if (!fuzz_initialized) {
            fuzz_reader = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);
            fuzz_initialized = true;
        }

        if (FR_remaining(&fuzz_reader) < 50) {
            return;
        }

        uint8_t numQueues = (uint8_t)FR_next_range(&fuzz_reader, 1, MAX_QUEUES);
        QueueHandle_t handles[MAX_QUEUES];
        char name_bufs[MAX_QUEUES][NAME_LEN];

        for (uint8_t j = 0; j < MAX_QUEUES; j++) {
            handles[j] = NULL;
        }

        for (uint8_t j = 0; j < numQueues; j++) {
            handles[j] = xQueueCreateStatic(QUEUE_LEN, ITEM_SIZE, ucQueueStorage[j], &xStaticQueues[j]);
            if (handles[j] != NULL) {
                FR_next_string(&fuzz_reader, name_bufs[j], NAME_LEN);
                if (FR_next_u8(&fuzz_reader) % 2 == 0) {
                    vQueueAddToRegistry(handles[j], name_bufs[j]);
                }
            }
        }

        uint8_t pick = (uint8_t)FR_next_range(&fuzz_reader, 0, numQueues - 1);
        if (handles[pick] != NULL) {
            (void)pcQueueGetName(handles[pick]);
        }

        for (uint8_t j = 0; j < numQueues; j++) {
            if (handles[j] != NULL) {
                vQueueUnregisterQueue(handles[j]);
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
