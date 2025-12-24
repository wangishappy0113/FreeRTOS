/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: vQueueDelete_static_queue_lifecycle_fuzz
 * 生成时间: 2025-12-19 00:00:52
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: queue
 */

/*
 * 测试用例详细信息:
 * 名称: vQueueDelete_static_queue_lifecycle_fuzz
 * 描述: Fuzzes vQueueDelete using statically allocated queues and semaphores with varied lengths, item sizes, and pre-delete send/receive operations, ensuring no tasks are blocked on the queue at deletion time.
 * 
 * 生成上下文:
 * 检测到的API函数: SCB_DFSR_EXTERNAL_Msk              , __set_PSP, CMSDK_PL230_DMA_WAITONREQ_STATUS_Msk       , Interrupts , TRC_RECORDER_MODE_STREAMING , SysTick_CALIB_TENMS_Msk            , CMSDK_GPIO_INTTYPECLR_Msk      , __TZ_set_PSP_NS, ISRs , SCB_CFSR_STKERR_Pos               
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
typedef struct
{
    StaticQueue_t queue_struct;
    uint8_t storage[64];
    QueueHandle_t handle;
    BaseType_t in_use;
} FuzzStaticQueueSlot_t;

#define FUZZ_MAX_SLOTS 4

static FuzzStaticQueueSlot_t g_slots[FUZZ_MAX_SLOTS];

static void fuzz_init_slots(void)
{
    for (int i = 0; i < FUZZ_MAX_SLOTS; i++)
    {
        g_slots[i].handle = NULL;
        g_slots[i].in_use = pdFALSE;
    }
}

static QueueHandle_t fuzz_create_queue_from_fuzz(FR_Reader *fr, BaseType_t *is_semaphore)
{
    if (FR_remaining(fr) < 3)
    {
        return NULL;
    }

    uint8_t slot_index = FR_next_u8(fr) % FUZZ_MAX_SLOTS;
    uint8_t len_raw = FR_next_u8(fr);
    uint8_t item_raw = FR_next_u8(fr);

    FuzzStaticQueueSlot_t *slot = &g_slots[slot_index];

    /* If slot already in use, delete existing queue first to avoid leaks. */
    if (slot->in_use == pdTRUE && slot->handle != NULL)
    {
        vQueueDelete(slot->handle);
        slot->handle = NULL;
        slot->in_use = pdFALSE;
    }

    /* Map fuzz bytes to queue parameters. */
    UBaseType_t length = (UBaseType_t)(len_raw % 8u); /* 0-7 */
    if (length == 0u)
    {
        length = 1u;
    }

    UBaseType_t item_size = (UBaseType_t)(item_raw % 17u); /* 0-16 */

    /* Decide between normal queue and binary semaphore style. */
    uint8_t kind = (uint8_t)(length + item_size); /* mix bits */
    *is_semaphore = (kind & 0x1u) ? pdTRUE : pdFALSE;

    QueueHandle_t q = NULL;

    if (*is_semaphore == pdTRUE)
    {
        /* Binary semaphore: length 1, item size 0. Use semaphore API. */
        length = 1u;
        item_size = 0u;
        q = xSemaphoreCreateBinaryStatic(&slot->queue_struct);
    }
    else
    {
        /* Normal queue: ensure storage fits. */
        size_t max_items = sizeof(slot->storage) / (item_size == 0u ? 1u : item_size);
        if (max_items == 0u)
        {
            return NULL;
        }
        if (length > max_items)
        {
            length = (UBaseType_t)max_items;
        }
        if (length == 0u)
        {
            length = 1u;
        }

        q = xQueueCreateStatic(length,
                               item_size,
                               slot->storage,
                               &slot->queue_struct);
    }

    if (q != NULL)
    {
        slot->handle = q;
        slot->in_use = pdTRUE;
    }

    return q;
}

static void fuzz_exercise_queue(FR_Reader *fr, QueueHandle_t q, BaseType_t is_semaphore)
{
    if (q == NULL)
    {
        return;
    }

    if (FR_remaining(fr) < 2)
    {
        return;
    }

    uint8_t ops = FR_next_u8(fr);
    uint8_t pattern = FR_next_u8(fr);

    UBaseType_t max_ops = (UBaseType_t)(ops % 6u); /* up to 5 operations */
    if (max_ops == 0u)
    {
        max_ops = 1u;
    }

    for (UBaseType_t i = 0; i < max_ops; i++)
    {
        BaseType_t do_send = ((pattern >> (i & 0x7u)) & 0x1u) ? pdTRUE : pdFALSE;

        if (is_semaphore == pdTRUE)
        {
            if (do_send == pdTRUE)
            {
                (void)xSemaphoreGive(q);
            }
            else
            {
                (void)xSemaphoreTake(q, 0u);
            }
        }
        else
        {
            /* For normal queues, send/receive single byte items. */
            uint8_t data = (uint8_t)(pattern + i);
            if (do_send == pdTRUE)
            {
                (void)xQueueSend(q, &data, 0u);
            }
            else
            {
                uint8_t out = 0u;
                (void)xQueueReceive(q, &out, 0u);
            }
        }
    }
}

static void fuzz_delete_queue(QueueHandle_t q)
{
    if (q == NULL)
    {
        return;
    }

    /* Find slot and mark it free after delete. */
    for (int i = 0; i < FUZZ_MAX_SLOTS; i++)
    {
        if (g_slots[i].handle == q)
        {
            vQueueDelete(q);
            g_slots[i].handle = NULL;
            g_slots[i].in_use = pdFALSE;
            return;
        }
    }

    /* If not found in slots, still delete to keep semantics correct. */
    vQueueDelete(q);
}


// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
static void test_task_impl(void *pvParameters)
{
    (void)pvParameters;

    // 测试用例: vQueueDelete_static_queue_lifecycle_fuzz
    // API类别: queue
    // 描述: Fuzzes vQueueDelete using statically allocated queues and semaphores with varied lengths, item sizes, and pre-delete send/receive operations, ensuring no tasks are blocked on the queue at deletion time.

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
        vTaskDelete(NULL);
    }

    for (unsigned int i = 0; i < iterations; ++i) {
        static BaseType_t initialized = pdFALSE;
        static FR_Reader local_fr;

        if (initialized == pdFALSE)
        {
            local_fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);
            fuzz_init_slots();
            initialized = pdTRUE;
        }

        /* Ensure we have enough bytes to derive parameters; stop if exhausted. */
        if (FR_remaining(&local_fr) < 16)
        {
            break;
        }

        /* Derive three independent parameters from fuzz input. */
        uint8_t selector = FR_next_u8(&local_fr);
        uint8_t pre_ops_flag = FR_next_u8(&local_fr);
        uint8_t extra_mix = FR_next_u8(&local_fr);

        (void)extra_mix; /* Currently only used to vary control flow lightly. */

        BaseType_t is_semaphore = pdFALSE;
        QueueHandle_t q = NULL;

        /* Use selector to decide whether to create a new queue or reuse a slot. */
        if ((selector & 0x1u) != 0u)
        {
            q = fuzz_create_queue_from_fuzz(&local_fr, &is_semaphore);
        }
        else
        {
            /* Try to pick an existing slot if any in use. */
            uint8_t idx = (uint8_t)(selector % FUZZ_MAX_SLOTS);
            if (g_slots[idx].in_use == pdTRUE)
            {
                q = g_slots[idx].handle;
                /* Heuristically treat queues with at most one message as semaphore-like. */
                if (uxQueueMessagesWaiting(q) <= 1u)
                {
                    is_semaphore = pdTRUE;
                }
            }
            else
            {
                q = fuzz_create_queue_from_fuzz(&local_fr, &is_semaphore);
            }
        }

        if (q == NULL)
        {
            /* Cannot proceed without a valid queue. */
            break;
        }

        /* Optionally exercise the queue before deletion. */
        if ((pre_ops_flag & 0x1u) != 0u)
        {
            fuzz_exercise_queue(&local_fr, q, is_semaphore);
        }

        /* No tasks are blocked on this queue in this harness, so safe to delete. */
        fuzz_delete_queue(q);
    }

    printf("[TEST_CASE_COMPLETED]\n");
    fflush(stdout);

    BREAKPOINT();
}

// 保持原有符号名 test_task 供外部引用，但适配 FreeRTOS TaskFunction_t 签名
void __attribute__((used, visibility("default"))) test_task(void *pvParameters)
{
    test_task_impl(pvParameters);
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
