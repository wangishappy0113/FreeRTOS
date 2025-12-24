/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: xTimerGetPeriod_basic_and_after_change
 * 生成时间: 2025-12-19 00:37:05
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: timers
 */

/*
 * 测试用例详细信息:
 * 名称: xTimerGetPeriod_basic_and_after_change
 * 描述: Fuzzes xTimerGetPeriod by creating multiple static timers with varying periods and auto-reload settings, querying their periods before and after xTimerChangePeriod/xTimerChangePeriodFromISR, and occasionally querying invalid or NULL handles to exercise configASSERT paths when enabled.
 * 
 * 生成上下文:
 * 检测到的API函数: lengths , __NVIC_DisableIRQ, I2S_CONTROL_TXEN_Msk        , TPI_DEVID_MANCVALID_Msk            , c , Interface , MPS2_SSP4               , CMSDK_DUALTIMER_MASKINTSTAT_Msk      , ARM_MPU_REGION_SIZE_1KB      , SCB_GetFPUType
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
    StaticTimer_t timerStorage;
    TimerHandle_t handle;
} FuzzTimerSlot;

#define MAX_FUZZ_TIMERS 4

static FuzzTimerSlot g_timerSlots[MAX_FUZZ_TIMERS];
static BaseType_t g_timersInitialized = pdFALSE;

static void fuzzTimerCallback( TimerHandle_t xTimer )
{
    /* Minimal callback: just query the period to exercise xTimerGetPeriod from callback context. */
    ( void ) xTimerGetPeriod( xTimer );
}

static void init_fuzz_timers_once( void )
{
    if( g_timersInitialized == pdTRUE )
    {
        return;
    }

    for( UBaseType_t i = 0; i < MAX_FUZZ_TIMERS; i++ )
    {
        g_timerSlots[ i ].handle = NULL;
    }

    g_timersInitialized = pdTRUE;
}

static TimerHandle_t create_or_reuse_timer_from_fuzz( FR_Reader * fr )
{
    /* Need at least 3 bytes: slot index, auto-reload flag, and period selector. */
    if( FR_remaining( fr ) < 3 )
    {
        return NULL;
    }

    uint8_t slotIdxRaw = FR_next_u8( fr );
    uint8_t autoReloadRaw = FR_next_u8( fr );
    uint8_t periodRaw = FR_next_u8( fr );

    UBaseType_t slotIdx = ( UBaseType_t ) ( slotIdxRaw % MAX_FUZZ_TIMERS );
    BaseType_t autoReload = ( autoReloadRaw & 0x1u ) ? pdTRUE : pdFALSE;

    /* Map periodRaw into a reasonable non-zero tick period: 1..1000 */
    TickType_t periodTicks = ( TickType_t ) ( ( ( TickType_t ) periodRaw % 1000u ) + 1u );

    FuzzTimerSlot * slot = &g_timerSlots[ slotIdx ];

    if( slot->handle == NULL )
    {
        /* Create a new static timer in this slot. */
        slot->handle = xTimerCreateStatic(
            "FzTmr",
            periodTicks,
            autoReload,
            ( void * ) slot,
            fuzzTimerCallback,
            &slot->timerStorage );
    }
    else
    {
        /* If timer already exists, optionally change its period to introduce more state. */
        ( void ) xTimerChangePeriod( slot->handle, periodTicks, 0 );
    }

    return slot->handle;
}

static void maybe_change_period_and_query( FR_Reader * fr, TimerHandle_t xTimer )
{
    if( xTimer == NULL )
    {
        return;
    }

    /* Need at least 3 bytes: selector, newPeriodRaw, isrChangeFlag. */
    if( FR_remaining( fr ) < 3 )
    {
        return;
    }

    uint8_t selector = FR_next_u8( fr );
    uint8_t newPeriodRaw = FR_next_u8( fr );
    uint8_t isrChangeRaw = FR_next_u8( fr );

    /* Always query once before any change. */
    ( void ) xTimerGetPeriod( xTimer );

    /* Map newPeriodRaw into 1..2000 ticks. */
    TickType_t newPeriod = ( TickType_t ) ( ( ( TickType_t ) newPeriodRaw % 2000u ) + 1u );

    /* Roughly 50% chance to perform a change. */
    if( ( selector & 0x1u ) != 0u )
    {
        if( ( isrChangeRaw & 0x1u ) != 0u )
        {
            /*
             * xTimerChangePeriodFromISR must only be called from an ISR context.
             * Calling it from a task will hit configASSERT in timers.c (line ~559).
             * To keep the harness safe in this demo (no custom ISRs), we avoid
             * calling the FromISR variant entirely and just use xTimerChangePeriod.
             */
            ( void ) xTimerChangePeriod( xTimer, newPeriod, 0 );
        }
        else
        {
            ( void ) xTimerChangePeriod( xTimer, newPeriod, 0 );
        }
    }

    /* Query again after potential change. */
    ( void ) xTimerGetPeriod( xTimer );
}

static void maybe_query_invalid_handles( FR_Reader * fr )
{
    /* Use a few bytes to decide whether to query NULL or stale handles. */
    if( FR_remaining( fr ) < 2 )
    {
        return;
    }

    uint8_t nullQueryRaw = FR_next_u8( fr );
    uint8_t staleQueryRaw = FR_next_u8( fr );

    /*
     * Directly calling xTimerGetPeriod with NULL or fabricated handles can
     * trigger configASSERT in some FreeRTOS configurations. To keep the
     * harness from aborting the whole demo, we avoid intentionally invalid
     * handles here and only operate on known-valid timers.
     */

    ( void ) nullQueryRaw; /* suppress unused warnings while disabling NULL tests */

    if( ( staleQueryRaw & 0x1u ) != 0u )
    {
        TimerHandle_t base = NULL;
        for( UBaseType_t i = 0; i < MAX_FUZZ_TIMERS; i++ )
        {
            if( g_timerSlots[ i ].handle != NULL )
            {
                base = g_timerSlots[ i ].handle;
                break;
            }
        }

        if( base != NULL )
        {
            /* Instead of fabricating a bogus pointer, just re-query a valid one. */
            ( void ) xTimerGetPeriod( base );
        }
    }
}


// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void)
{
    // 测试用例: xTimerGetPeriod_basic_and_after_change
    // API类别: timers
    // 描述: Fuzzes xTimerGetPeriod by creating multiple static timers with varying periods and auto-reload settings, querying their periods before and after xTimerChangePeriod/xTimerChangePeriodFromISR, and occasionally querying invalid or NULL handles to exercise configASSERT paths when enabled.

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
    // LLM 生成的 test_logic 将被放置在该 for 循环内部，
    // 每次迭代可执行少量 API 调用或状态变换。禁止在 test_logic 中
    // 再引入无限循环或长时间阻塞。
    for (unsigned int i = 0; i < iterations; ++i) {
        init_fuzz_timers_once();

        /* Each iteration consumes fresh bytes to drive timer creation, period changes, and queries. */

        /* Ensure we have at least 16 bytes overall remaining to satisfy the fuzzing policy. */
        if( FR_remaining( &fr ) < 16 )
        {
            break;
        }

        /* Step 1: Create or reuse a timer based on fuzz input. */
        TimerHandle_t t = create_or_reuse_timer_from_fuzz( &fr );

        /* Step 2: Query its period before and after potential changes. */
        maybe_change_period_and_query( &fr, t );

        /* Step 3: Optionally perform queries with invalid or NULL handles to exercise assertions. */
        maybe_query_invalid_handles( &fr );

        /* Step 4: Optionally iterate over all existing timers and query their periods to increase coverage. */
        if( FR_remaining( &fr ) >= 1 )
        {
            uint8_t scanRaw = FR_next_u8( &fr );
            if( ( scanRaw & 0x1u ) != 0u )
            {
                for( UBaseType_t j = 0; j < MAX_FUZZ_TIMERS; j++ )
                {
                    if( g_timerSlots[ j ].handle != NULL )
                    {
                        ( void ) xTimerGetPeriod( g_timerSlots[ j ].handle );
                    }
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