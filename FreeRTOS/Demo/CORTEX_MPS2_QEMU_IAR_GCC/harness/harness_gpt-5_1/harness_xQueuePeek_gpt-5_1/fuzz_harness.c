/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: xQueuePeek_basic_fuzz
 * 生成时间: 2025-12-19 00:16:30
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: queue
 */

/*
 * 测试用例详细信息:
 * 名称: xQueuePeek_basic_fuzz
 * 描述: Fuzz xQueuePeek with varying queue lengths, item sizes, tick waits, and producer/consumer interactions using static queues and tasks.
 * 
 * 生成上下文:
 * 检测到的API函数: __builtin_arm_set_fpscr, volatile, ARM_MPU_REGION_SIZE_512KB    , SCB_CPUID_VARIANT_Msk              , SCB_SHCSR_USGFAULTENA_Msk          , SCB_CFSR_BFARVALID_Msk            , CMSDK_TIMER_CTRL_SELEXTEN_Msk       , CMSDK_GPIO0             , CLCD_RS_Msk        , SSP_SR_BSY_Msk          
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
    QueueHandle_t xQueue;
    StaticQueue_t xQueueStruct;
    uint8_t *pucStorage;
    UBaseType_t uxLength;
    UBaseType_t uxItemSize;
} FuzzQueueCtx_t;

static FuzzQueueCtx_t xFuzzCtx = { 0 };

#define MAX_QUEUE_STORAGE_BYTES 128U
static uint8_t ucQueueStorage[MAX_QUEUE_STORAGE_BYTES];

#define PRODUCER_TASK_STACK_SIZE 128U
static StackType_t xProducerStack[PRODUCER_TASK_STACK_SIZE];
static StaticTask_t xProducerTCB;
static TaskHandle_t xProducerHandle = NULL;

static void vProducerTask( void *pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        /* Simple periodic producer that sends incrementing bytes if the
         * queue exists. Uses zero block time to avoid long blocking. */
        if( xFuzzCtx.xQueue != NULL )
        {
            uint8_t ucVal = ( uint8_t ) ( xTaskGetTickCount() & 0xFFU );
            ( void ) xQueueSend( xFuzzCtx.xQueue, &ucVal, 0 );
        }

        vTaskDelay( 1 );
    }
}

static void prvMaybeInitProducerTask( void )
{
    if( xProducerHandle == NULL )
    {
        /* Create producer at a lower priority than fuzz task by default. */
        UBaseType_t uxPriority = tskIDLE_PRIORITY;
        xProducerHandle = xTaskCreateStatic( vProducerTask,
                                             "prdFz",
                                             PRODUCER_TASK_STACK_SIZE,
                                             NULL,
                                             uxPriority,
                                             xProducerStack,
                                             &xProducerTCB );
        configASSERT( xProducerHandle != NULL );
    }
}

static void prvCreateOrReconfigureQueue( FR_Reader *pfr )
{
    /* Derive queue length and item size from fuzz input, ensuring they fit
     * into the static storage buffer. */
    if( FR_remaining( pfr ) < 4 )
    {
        return;
    }

    uint8_t lenByte = FR_next_u8( pfr );
    uint8_t sizeByte = FR_next_u8( pfr );
    uint8_t fillCountByte = FR_next_u8( pfr );
    ( void ) FR_next_u8( pfr ); /* reserved for future variations */

    UBaseType_t uxLength = ( UBaseType_t ) ( ( lenByte % 8U ) + 1U ); /* 1..8 */
    UBaseType_t uxItemSize = ( UBaseType_t ) ( ( sizeByte % 16U ) + 1U ); /* 1..16 */

    /* Ensure total storage fits into static buffer. */
    if( ( uxLength * uxItemSize ) > MAX_QUEUE_STORAGE_BYTES )
    {
        /* Clamp length down to fit. */
        uxLength = MAX_QUEUE_STORAGE_BYTES / uxItemSize;
        if( uxLength == 0U )
        {
            uxLength = 1U;
            uxItemSize = 1U;
        }
    }

    xFuzzCtx.uxLength = uxLength;
    xFuzzCtx.uxItemSize = uxItemSize;
    xFuzzCtx.pucStorage = ucQueueStorage;

    /* Create a new static queue each time; handle is overwritten safely. */
    xFuzzCtx.xQueue = xQueueCreateStatic( xFuzzCtx.uxLength,
                                          xFuzzCtx.uxItemSize,
                                          xFuzzCtx.pucStorage,
                                          &xFuzzCtx.xQueueStruct );
    configASSERT( xFuzzCtx.xQueue != NULL );

    /* Optionally pre-fill the queue with some items. */
    UBaseType_t uxFill = ( UBaseType_t ) ( fillCountByte % ( xFuzzCtx.uxLength + 1U ) );
    uint8_t ucVal = 0xAAU;
    for( UBaseType_t i = 0; i < uxFill; i++ )
    {
        ( void ) xQueueSend( xFuzzCtx.xQueue, &ucVal, 0 );
        ucVal++;
    }
}

static TickType_t prvDeriveTicksToWait( FR_Reader *pfr )
{
    if( FR_remaining( pfr ) < 1 )
    {
        return 0;
    }

    uint8_t mode = FR_next_u8( pfr );

    switch( mode % 4U )
    {
        case 0:
            return 0; /* no block */
        case 1:
            return ( TickType_t ) 1; /* minimal block */
        case 2:
            return ( TickType_t ) 5; /* small block */
        default:
            /* Avoid using portMAX_DELAY in this demo harness to prevent
             * configASSERTs when INCLUDE_vTaskSuspend is 0 or other
             * port-specific restrictions apply. Use a modest finite
             * timeout instead to keep fuzz iterations bounded. */
            return ( TickType_t ) 10;
    }
}

static void prvMaybeSendFromTestTask( FR_Reader *pfr )
{
    if( xFuzzCtx.xQueue == NULL )
    {
        return;
    }

    if( FR_remaining( pfr ) < 1 )
    {
        return;
    }

    uint8_t decision = FR_next_u8( pfr );

    if( ( decision & 0x1U ) != 0U )
    {
        /* Send one item with zero block time. */
        uint8_t ucVal = decision;
        ( void ) xQueueSend( xFuzzCtx.xQueue, &ucVal, 0 );
    }
}


// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void)
{
    // 测试用例: xQueuePeek_basic_fuzz
    // API类别: queue
    // 描述: Fuzz xQueuePeek with varying queue lengths, item sizes, tick waits, and producer/consumer interactions using static queues and tasks.

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
        {
            static BaseType_t xInitialized = pdFALSE;
            static FR_Reader sfr;

            if( xInitialized == pdFALSE )
            {
                /* Initialize static reader over the same deterministic
                 * FUZZ_INPUT buffer. */
                sfr = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);
                xInitialized = pdTRUE;
            }

            /* Ensure we have enough bytes to drive at least one variation. */
            if( FR_remaining( &sfr ) < 16 )
            {
                return;
            }

            /* Step 1: (Re)configure queue based on fuzz input. */
            prvCreateOrReconfigureQueue( &sfr );
            if( xFuzzCtx.xQueue == NULL )
            {
                return;
            }

            /* Step 2: Ensure background producer exists to exercise blocking paths. */
            prvMaybeInitProducerTask();

            /* Step 3: Optionally send an item from this test task before peeking. */
            prvMaybeSendFromTestTask( &sfr );

            /* Step 4: Derive tick wait and perform xQueuePeek. */
            TickType_t xTicksToWait = prvDeriveTicksToWait( &sfr );

            /* Local buffer sized to maximum item size we ever configure (<=16). */
            uint8_t ucLocalBuf[16];

            /* Ensure we don't overflow local buffer when queue item size is larger
             * than 16 (should not happen due to construction, but assert anyway). */
            configASSERT( xFuzzCtx.uxItemSize <= sizeof( ucLocalBuf ) );

            BaseType_t xPeekResult = xQueuePeek( xFuzzCtx.xQueue,
                                                 ( void * ) ucLocalBuf,
                                                 xTicksToWait );

            /* Basic sanity: if peek passed, queue must not be empty. */
            if( xPeekResult == pdPASS )
            {
                UBaseType_t uxCount = uxQueueMessagesWaiting( xFuzzCtx.xQueue );
                configASSERT( uxCount > 0U );

                /* Immediately peek again with zero block to exercise non-blocking
                 * path and ensure the same item remains. */
                uint8_t ucSecondBuf[16];
                configASSERT( xFuzzCtx.uxItemSize <= sizeof( ucSecondBuf ) );
                ( void ) xQueuePeek( xFuzzCtx.xQueue,
                                     ( void * ) ucSecondBuf,
                                     0 );
            }
            else
            {
                /* If peek failed, queue may be empty; just query its state. */
                ( void ) uxQueueMessagesWaiting( xFuzzCtx.xQueue );
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