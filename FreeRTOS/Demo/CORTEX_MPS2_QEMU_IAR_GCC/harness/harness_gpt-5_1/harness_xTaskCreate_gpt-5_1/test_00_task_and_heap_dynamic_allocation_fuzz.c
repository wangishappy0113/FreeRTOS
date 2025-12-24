/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: task_and_heap_dynamic_allocation_fuzz
 * 生成时间: 2025-12-18 20:41:13
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: task_and_heap_dynamic_allocation_fuzz
 * 描述: Fuzzes dynamic heap allocation via pvPortMalloc and task creation via xTaskCreate by deriving sizes, priorities, and parameters from FUZZ_INPUT. Exercises allocation success/failure paths, task startup, and deletion while ensuring memory safety and static object usage in the harness.
 * 
 * 生成上下文:
 * 检测到的API函数: CFSR , MPS2_VGA_BUFFER         , CoreDebug_DEMCR_MON_REQ_Msk        , CoreDebug           , CMSDK_UART2_BASE        , __SMLALDX , SSP_CR0_SPO_Msk         , xPSR_C_Msk                         , SCB_HFSR_DEBUGEVT_Msk              , CMSDK_TIMER0_BASE       
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
    uint8_t pattern;
    uint8_t index;
    uint16_t payload;
} FuzzTaskParam_t;

static void fuzz_worker_task( void * pvParameters )
{
    FuzzTaskParam_t * p = ( FuzzTaskParam_t * ) pvParameters;

    if( p != NULL )
    {
        /* Perform a few simple deterministic operations based on the fuzzed
         * parameters to exercise the task context and stack without looping
         * forever. */
        volatile uint32_t acc = 0U;
        acc += ( uint32_t ) p->pattern;
        acc ^= ( uint32_t ) p->index;
        acc += ( uint32_t ) p->payload;

        /* Use the accumulator in a conditional to avoid being optimized away. */
        if( acc == 0xFFFFFFFFUL )
        {
            /* Extremely unlikely, but keeps the compiler from removing code. */
            ( void ) acc;
        }
    }

    /* Return from the task function to allow the scheduler to clean up if the
     * task is deleted by the test harness. */
    vTaskDelete( NULL );
}


// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void)
{
    // 测试用例: task_and_heap_dynamic_allocation_fuzz
    // API类别: task_management
    // 描述: Fuzzes dynamic heap allocation via pvPortMalloc and task creation via xTaskCreate by deriving sizes, priorities, and parameters from FUZZ_INPUT. Exercises allocation success/failure paths, task startup, and deletion while ensuring memory safety and static object usage in the harness.

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
            /* Each iteration consumes additional fuzz bytes; if we ever run out,
             * break out of the fuzz loop by returning to the caller, which will
             * stop invoking test_logic. */

            /* Derive a task name from fuzz input. Require at least 4 bytes for a
             * minimal, non-empty name plus terminator. */
            char taskName[ configMAX_TASK_NAME_LEN ];
            if( FR_remaining( &fr ) < 4 )
            {
                return;
            }

            /* Generate a variable-length name between 1 and configMAX_TASK_NAME_LEN-1. */
            uint8_t nameLenByte = FR_next_u8( &fr );
            size_t maxNameLen = ( configMAX_TASK_NAME_LEN > 1U ) ? ( ( size_t ) configMAX_TASK_NAME_LEN - 1U ) : 1U;
            size_t nameLen = ( ( size_t ) nameLenByte % maxNameLen ) + 1U;

            /* Fill the name from fuzz data; if insufficient bytes remain, abort. */
            if( FR_remaining( &fr ) < nameLen )
            {
                return;
            }
            FR_next_bytes( &fr, ( uint8_t * ) taskName, nameLen );
            taskName[ nameLen ] = '\0';

            /* Derive stack depth in words from fuzz input. Ensure it is at least
             * configMINIMAL_STACK_SIZE and clamp to a reasonable upper bound to
             * avoid exhausting the heap too aggressively. */
            if( FR_remaining( &fr ) < 2 )
            {
                return;
            }
            uint16_t rawDepth = FR_next_u16( &fr );
            configSTACK_DEPTH_TYPE minDepth = configMINIMAL_STACK_SIZE;
            configSTACK_DEPTH_TYPE maxDepth = ( configMINIMAL_STACK_SIZE * 8U );
            if( maxDepth < minDepth )
            {
                maxDepth = minDepth;
            }
            configSTACK_DEPTH_TYPE uxStackDepth = ( configSTACK_DEPTH_TYPE ) ( ( rawDepth % ( maxDepth - minDepth + 1U ) ) + minDepth );

            /* Derive task priority from fuzz input, clamped to valid range. */
            if( FR_remaining( &fr ) < 1 )
            {
                return;
            }
            uint8_t rawPrio = FR_next_u8( &fr );
            UBaseType_t uxPriority;
            if( configMAX_PRIORITIES > 0U )
            {
                uxPriority = ( UBaseType_t ) ( rawPrio % ( UBaseType_t ) configMAX_PRIORITIES );
            }
            else
            {
                uxPriority = 0U;
            }

            /* Derive a small parameter structure for the task from fuzz input. */
            if( FR_remaining( &fr ) < 4 )
            {
                return;
            }
            FuzzTaskParam_t param;
            param.pattern = FR_next_u8( &fr );
            param.index   = FR_next_u8( &fr );
            param.payload = FR_next_u16( &fr );

            /* Derive a heap allocation size to exercise pvPortMalloc. Use a modest
             * upper bound to avoid exhausting the heap too quickly. */
            if( FR_remaining( &fr ) < 2 )
            {
                return;
            }
            uint16_t rawAlloc = FR_next_u16( &fr );
            size_t maxAlloc = 256U;
            size_t allocSize = ( size_t ) ( rawAlloc % ( maxAlloc + 1U ) );
            if( allocSize == 0U )
            {
                /* Ensure we sometimes request non-zero allocations to hit the main
                 * pvPortMalloc path. */
                allocSize = 1U;
            }

            /* Perform the heap allocation using pvPortMalloc. */
            void * pMem = pvPortMalloc( allocSize );

            if( pMem != NULL )
            {
                /* Touch the allocated memory to ensure it is writable and to
                 * exercise alignment and block splitting paths. */
                uint8_t * pBytes = ( uint8_t * ) pMem;
                size_t i;
                for( i = 0U; i < allocSize; i++ )
                {
                    pBytes[ i ] = ( uint8_t ) ( ( i + param.pattern ) & 0xFFU );
                }
            }

            /* Create a task dynamically using xTaskCreate to exercise prvCreateTask
             * and the dynamic allocation paths. The created task will delete itself
             * in fuzz_worker_task. */
            TaskHandle_t xCreated = NULL;

            BaseType_t xRes = xTaskCreate( fuzz_worker_task,
                                           taskName,
                                           uxStackDepth,
                                           ( void * ) &param,
                                           uxPriority,
                                           &xCreated );

            /* If the task was created, optionally perform a simple interaction and
             * then delete it from this context to exercise deletion of a running or
             * ready task. */
            if( xRes == pdPASS )
            {
                configASSERT( xCreated != NULL );

                /* Optionally yield or delay zero ticks based on fuzz input to
                 * exercise different scheduling points. */
                if( FR_remaining( &fr ) >= 1 )
                {
                    uint8_t schedByte = FR_next_u8( &fr );
                    if( ( schedByte & 0x1U ) != 0U )
                    {
                        taskYIELD();
                    }
                    else
                    {
                        vTaskDelay( 0U );
                    }
                }

                /* Delete the task explicitly if it has not already deleted itself. */
                vTaskDelete( xCreated );
            }

            /* Free the allocated heap block, if any, to avoid leaks across
             * iterations. */
            if( pMem != NULL )
            {
                vPortFree( pMem );
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