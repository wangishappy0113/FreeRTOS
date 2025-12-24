/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: uxTaskGetSystemState_and_vTaskSuspendAll_fuzz
 * 生成时间: 2025-12-18 22:31:39
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: uxTaskGetSystemState_and_vTaskSuspendAll_fuzz
 * 描述: Fuzzes uxTaskGetSystemState and vTaskSuspendAll by creating a variable number of static tasks, optionally suspending the scheduler in nested fashion, and querying system state with varying array sizes and optional run-time counter pointer.
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

// LibAFL 全局输入缓冲区
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

// LibAFL 断点函数
int __attribute__((noinline, used, visibility("default"))) BREAKPOINT(void)
{
    for( ;; )
    {
        __asm volatile ("nop");
    }
}

#define FUZZ_TASK_STACK_DEPTH ( configMINIMAL_STACK_SIZE * 4 )
static StackType_t xFuzzTaskStack[ FUZZ_TASK_STACK_DEPTH ];
static StaticTask_t xFuzzTaskTCB;
#define FUZZ_TASK_PRIORITY ( tskIDLE_PRIORITY + 1 )

// ====================================================================
// Fuzz Reader (FR_*)
// ====================================================================
typedef struct FR_Reader
{
    const unsigned char * data;
    size_t size;
    size_t off;
} FR_Reader;

#define FR_init_SELECT( _1, _2, NAME, ... ) NAME
#define FR_init( ... ) FR_init_SELECT( __VA_ARGS__, FR_init_2, FR_init_1 )( __VA_ARGS__ )

static inline FR_Reader FR_init_1( const unsigned char * buf )
{
    FR_Reader r = { buf, MAX_FUZZ_INPUT_SIZE, 0 }; return r;
}

static inline FR_Reader FR_init_2( const unsigned char * buf, size_t n )
{
    FR_Reader r = { buf, n, 0 }; return r;
}

static inline size_t FR_remaining( FR_Reader * r )
{
    return ( r->off < r->size ) ? ( r->size - r->off ) : 0U;
}

static inline uint8_t FR_next_u8( FR_Reader * r )
{
    if( r->off + 1U <= r->size )
    {
        return r->data[ r->off++ ];
    }
    return 0U;
}

static inline uint16_t FR_next_u16( FR_Reader * r )
{
    uint16_t lo = FR_next_u8( r );
    uint16_t hi = FR_next_u8( r );
    return ( uint16_t ) ( ( hi << 8 ) | lo );
}

static inline uint32_t FR_next_u32( FR_Reader * r )
{
    uint32_t lo = FR_next_u16( r );
    uint32_t hi = FR_next_u16( r );
    return ( hi << 16 ) | lo;
}

static inline uint32_t FR_next_range( FR_Reader * r, uint32_t min_v, uint32_t max_v )
{
    if( max_v <= min_v )
    {
        return min_v;
    }
    uint32_t span = max_v - min_v + 1U;
    return min_v + ( FR_next_u32( r ) % span );
}

static inline size_t FR_next_bytes( FR_Reader * r, unsigned char * out, size_t n )
{
    size_t rem = FR_remaining( r );

    if( n > rem )
    {
        n = rem;
    }

    if( n != 0U )
    {
        memcpy( out, r->data + r->off, n );
        r->off += n;
    }

    return n;
}

// ====================================================================
// 本测试用例的辅助函数（非 FR_ 前缀）
// ====================================================================

/* Maximum number of test tasks we will ever create. */
#define MAX_TEST_TASKS 6U

/* Static storage for test tasks. */
static StaticTask_t xTestTaskTCBs[ MAX_TEST_TASKS ];
static StackType_t xTestTaskStacks[ MAX_TEST_TASKS ][ configMINIMAL_STACK_SIZE ];
static TaskHandle_t xTestTaskHandles[ MAX_TEST_TASKS ];

/* Simple test task function: it just delays/yields briefly to create
 * some scheduling activity without blocking forever. */
static void vSimpleTestTask( void * pvParameters )
{
    ( void ) pvParameters;

    for( UBaseType_t i = 0; i < 3U; i++ )
    {
        vTaskDelay( 1 );
        taskYIELD();
    }

    vTaskDelete( NULL );
}

/* Helper to (re)create up to uxNumTasks tasks if they are not already valid.
 * Uses static storage only. */
static void prvEnsureTestTasks( UBaseType_t uxNumTasks, UBaseType_t uxBasePriority )
{
    if( uxNumTasks > MAX_TEST_TASKS )
    {
        uxNumTasks = MAX_TEST_TASKS;
    }

    for( UBaseType_t i = 0; i < uxNumTasks; i++ )
    {
        if( xTestTaskHandles[ i ] == NULL )
        {
            UBaseType_t uxPriority = uxBasePriority + ( i % 2U );

            if( uxPriority >= configMAX_PRIORITIES )
            {
                uxPriority = configMAX_PRIORITIES - 1U;
            }

            TaskHandle_t xHandle = xTaskCreateStatic( vSimpleTestTask,
                                                      "FzTsk",
                                                      configMINIMAL_STACK_SIZE,
                                                      NULL,
                                                      uxPriority,
                                                      xTestTaskStacks[ i ],
                                                      &xTestTaskTCBs[ i ] );

            xTestTaskHandles[ i ] = xHandle;
        }
    }
}

/* Helper to perform a bounded nested suspend/resume sequence. */
static void prvDoNestedSuspendResume( UBaseType_t uxDepth )
{
    if( uxDepth == 0U )
    {
        return;
    }

    if( uxDepth > 4U )
    {
        uxDepth = 4U;
    }

    for( UBaseType_t i = 0; i < uxDepth; i++ )
    {
        vTaskSuspendAll();
    }

    for( UBaseType_t i = 0; i < uxDepth; i++ )
    {
        ( void ) xTaskResumeAll();
    }
}

// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task( void * pvParameters )
{
    ( void ) pvParameters;

    FR_Reader fr = FR_init( FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE );

    unsigned char fr_baseline[ 16 ] = { 0 };
    ( void ) FR_next_bytes( &fr, fr_baseline, sizeof( fr_baseline ) );

    unsigned int iterations = ( unsigned int ) FR_next_range( &fr, 0U, 10U );

    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        printf( "[ERROR] Scheduler not running - aborting test_task\n" );
        fflush( stdout );
        vTaskDelete( NULL );
    }

    for( unsigned int i = 0; i < iterations; i++ )
    {
        /* Ensure we have enough bytes to derive parameters; if not, exit iterations. */
        if( FR_remaining( &fr ) < 16U )
        {
            break;
        }

        /* Derive parameters from fuzz input. */
        uint8_t bNumTasks      = FR_next_u8( &fr );
        uint8_t bArraySize     = FR_next_u8( &fr );
        uint8_t bUseRunTimePtr = FR_next_u8( &fr );
        uint8_t bSuspendDepth  = FR_next_u8( &fr );
        uint8_t bBasePrio      = FR_next_u8( &fr );
        uint8_t bPreSuspend    = FR_next_u8( &fr );
        uint8_t bPostSuspend   = FR_next_u8( &fr );
        uint8_t bExtra         = FR_next_u8( &fr );

        ( void ) bExtra;

        UBaseType_t uxRequestedTasks = ( UBaseType_t ) ( bNumTasks % ( MAX_TEST_TASKS + 1U ) );
        UBaseType_t uxArraySize      = ( UBaseType_t ) ( bArraySize % ( MAX_TEST_TASKS + 2U ) );
        UBaseType_t uxBasePriority   = ( UBaseType_t ) ( bBasePrio % configMAX_PRIORITIES );
        UBaseType_t uxSuspendDepthPre  = ( UBaseType_t ) ( bPreSuspend % 5U );
        UBaseType_t uxSuspendDepthPost = ( UBaseType_t ) ( bPostSuspend % 5U );

        prvEnsureTestTasks( uxRequestedTasks, uxBasePriority );

        if( ( bSuspendDepth & 0x1U ) != 0U )
        {
            prvDoNestedSuspendResume( uxSuspendDepthPre );
        }

        static TaskStatus_t xStatusArray[ MAX_TEST_TASKS + 2U ];
        configRUN_TIME_COUNTER_TYPE ulTotalRunTime = 0U;
        configRUN_TIME_COUNTER_TYPE * pulRunTime   = NULL;

        if( ( bUseRunTimePtr & 0x1U ) != 0U )
        {
            pulRunTime = &ulTotalRunTime;
        }

        UBaseType_t uxPopulated = uxTaskGetSystemState( xStatusArray,
                                                         uxArraySize,
                                                         pulRunTime );

        if( uxArraySize > 0U )
        {
            configASSERT( uxPopulated <= uxArraySize );
        }

        if( pulRunTime != NULL )
        {
            ( void ) *pulRunTime;
        }

        if( ( bSuspendDepth & 0x2U ) != 0U )
        {
            prvDoNestedSuspendResume( uxSuspendDepthPost );
        }

        taskYIELD();
    }

    printf( "[TEST_CASE_COMPLETED]\n" );
    fflush( stdout );

    BREAKPOINT();
}

// FreeRTOS 任务包装器（保持签名和外部逻辑不变）
void fuzz_task( void )
{
    TaskHandle_t xHandle = xTaskCreateStatic( test_task,
                                              "FuzzTask",
                                              FUZZ_TASK_STACK_DEPTH,
                                              NULL,
                                              FUZZ_TASK_PRIORITY,
                                              xFuzzTaskStack,
                                              &xFuzzTaskTCB );

    if( xHandle == NULL )
    {
        printf( "FuzzTask creation failed\n" );
        fflush( stdout );
        configASSERT( 0 );
        for( ;; )
        {
        }
    }

    vTaskStartScheduler();

    for( ;; )
    {
    }
}
