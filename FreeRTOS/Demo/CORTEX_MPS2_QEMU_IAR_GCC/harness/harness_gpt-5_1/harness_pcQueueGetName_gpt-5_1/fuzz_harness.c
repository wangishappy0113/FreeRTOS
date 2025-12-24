/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: pcQueueGetName_basic_registry_fuzz
 * 生成时间: 2025-12-19 00:02:29
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: queue
 */

/*
 * 测试用例详细信息:
 * 名称: pcQueueGetName_basic_registry_fuzz
 * 描述: Fuzz pcQueueGetName by creating multiple static queues, optionally registering them with fuzz-derived names, then querying names for various handles (valid, unregistered, and NULL-like) to exercise registry search and NULL return paths.
 * 
 * 生成上下文:
 * 检测到的API函数: CMSDK_DUALTIMER1_CTRL_ONESHOOT_Msk   , __builtin_arm_get_fpscr, DWT_CTRL_NOTRCPKT_Msk              , xTimer1Handler, CMSDK_SYSCON_EMICTRL_WCYC_Msk          , CMSDK_UART_CTRL_TXORIRQEN_Msk     , CMSDK_TIMER1_BASE       , Interrupts , MPU_RNR_REGION_Msk                 , allocated 
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
static StaticQueue_t g_queues[4];
static uint8_t g_queue_storage[4][16];
static QueueHandle_t g_queue_handles[4];

static void init_static_queues_once(void)
{
    static BaseType_t initialized = pdFALSE;
    if(initialized == pdTRUE)
    {
        return;
    }

    for(UBaseType_t i = 0; i < 4; i++)
    {
        g_queue_handles[i] = xQueueCreateStatic(
            4,                          /* queue length */
            sizeof(uint8_t),            /* item size */
            g_queue_storage[i],         /* storage */
            &g_queues[i]);              /* control block */
        configASSERT(g_queue_handles[i] != NULL);
    }

    initialized = pdTRUE;
}

static void fuzz_register_queues(FR_Reader *fr)
{
    /* Use fuzz input to decide which queues to register and with what names. */
    for(UBaseType_t i = 0; i < 4; i++)
    {
        if(FR_remaining(fr) < 2)
        {
            return;
        }

        uint8_t do_register = FR_next_u8(fr);
        uint8_t name_len_hint = FR_next_u8(fr);

        if((do_register & 0x1u) == 0u)
        {
            continue; /* skip registration for this queue */
        }

        /* Derive a name length between 1 and 15 (leave room for NUL). */
        size_t max_len = 15u;
        size_t name_len = (size_t)(name_len_hint % (max_len == 0 ? 1u : max_len));
        if(name_len == 0u)
        {
            name_len = 1u;
        }

        if(FR_remaining(fr) < name_len)
        {
            return;
        }

        char name_buf[16];
        memset(name_buf, 0, sizeof(name_buf));
        /* Fill name with arbitrary bytes; ensure NUL termination. */
        for(size_t j = 0; j < name_len; j++)
        {
            name_buf[j] = (char)FR_next_u8(fr);
            if(name_buf[j] == '\0')
            {
                name_buf[j] = (char)('A' + (j % 26));
            }
        }
        name_buf[name_len] = '\0';

        vQueueAddToRegistry(g_queue_handles[i], name_buf);
    }
}

static QueueHandle_t fuzz_select_queue_handle(FR_Reader *fr)
{
    /* Choose between valid handles and some intentionally invalid/NULL-like values. */
    if(FR_remaining(fr) < 1)
    {
        return NULL;
    }

    uint8_t selector = FR_next_u8(fr);
    uint8_t mode = selector % 6u;

    switch(mode)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        {
            /* Valid queue handle from our array. */
            UBaseType_t idx = (UBaseType_t)(mode % 4u);
            return g_queue_handles[idx];
        }
        case 4:
            /* Explicit NULL to exercise configASSERT if enabled. */
            return (QueueHandle_t)NULL;
        case 5:
        default:
        {
            /* Corrupted/alien handle: offset a valid handle pointer. */
            UBaseType_t idx = (UBaseType_t)(selector % 4u);
            uintptr_t base = (uintptr_t)g_queue_handles[idx];
            uintptr_t fuzz_offset = (uintptr_t)(selector & 0xFCu); /* small aligned offset */
            return (QueueHandle_t)(base + fuzz_offset);
        }
    }
}


// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void *pvParameters)
{
    (void)pvParameters;

    // 测试用例: pcQueueGetName_basic_registry_fuzz
    // API类别: queue
    // 描述: Fuzz pcQueueGetName by creating multiple static queues, optionally registering them with fuzz-derived names, then querying names for various handles (valid, unregistered, and NULL-like) to exercise registry search and NULL return paths.

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
    // 每次迭代可执行少量 API 调用或状态变换。禁止在 test_logic 中
    // 再引入无限循环或长时间阻塞。
    for (unsigned int i = 0; i < iterations; ++i) {
        // 使用单个静态 Reader，在第一次迭代时初始化，避免变量名与外层 fr 冲突
        static BaseType_t inner_fr_initialized = pdFALSE;
        static FR_Reader inner_fr;

        if(inner_fr_initialized == pdFALSE)
        {
            inner_fr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);
            inner_fr_initialized = pdTRUE;
        }

        /* Ensure we have at least some bytes to drive this iteration. */
        if(FR_remaining(&inner_fr) < 16)
        {
            break; /* exit fuzz loop in harness */
        }

        /* One-time queue creation. */
        init_static_queues_once();

        /* Use fuzz input to (re)register queues with names. */
        fuzz_register_queues(&inner_fr);

        /* Derive how many queries to perform this iteration (1..4). */
        if(FR_remaining(&inner_fr) < 1)
        {
            break;
        }
        uint8_t query_count_raw = FR_next_u8(&inner_fr);
        UBaseType_t query_count = (UBaseType_t)((query_count_raw % 4u) + 1u);

        for(UBaseType_t q = 0; q < query_count; q++)
        {
            if(FR_remaining(&inner_fr) < 1)
            {
                break;
            }

            QueueHandle_t target = fuzz_select_queue_handle(&inner_fr);

            /* Call the function under test. Note: if target is NULL, configASSERT may trigger. */
            const char *name = pcQueueGetName(target);

            /* Basic sanity: returned pointer is either NULL or a plausible C string. */
            if(name != NULL)
            {
                /* Walk up to a small bound to avoid overruns if name is invalid. */
                size_t max_check = 32u;
                size_t len = 0u;
                while(len < max_check && name[len] != '\0')
                {
                    (void)name[len];
                    len++;
                }
            }
        }

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
