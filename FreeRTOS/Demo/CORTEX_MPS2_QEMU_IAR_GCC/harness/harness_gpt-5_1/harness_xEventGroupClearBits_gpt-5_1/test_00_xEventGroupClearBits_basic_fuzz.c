/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: xEventGroupClearBits_basic_fuzz
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: event_groups
 */

/*
 * 测试用例详细信息:
 * 名称: xEventGroupClearBits_basic_fuzz
 * 描述: Fuzzes xEventGroupClearBits by varying bits to set/clear and
 *        interleaving with waits and sets from a helper task, ensuring
 *        correct pre-clear return values and bit manipulation under
 *        concurrent access.
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

// LibAFL 集成必需的全局变量和函数
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

#define FUZZ_TASK_STACK_DEPTH ( configMINIMAL_STACK_SIZE * 4U )
static StackType_t xFuzzTaskStack[ FUZZ_TASK_STACK_DEPTH ];
static StaticTask_t xFuzzTaskTCB;
#define FUZZ_TASK_PRIORITY ( tskIDLE_PRIORITY + 1U )

// ====================================================================
// Fuzz Reader (FR_*)
// ====================================================================
typedef struct
{
    const unsigned char * data;
    size_t size;
    size_t off;
} FR_Reader;

#define FR_init_SELECT(_1,_2,NAME,...) NAME
#define FR_init(...) FR_init_SELECT(__VA_ARGS__, FR_init_2, FR_init_1)(__VA_ARGS__)

static inline FR_Reader FR_init_1( const unsigned char * buf )
{
    FR_Reader r = { buf, MAX_FUZZ_INPUT_SIZE, 0U };
    return r;
}

static inline FR_Reader FR_init_2( const unsigned char * buf, size_t n )
{
    FR_Reader r = { buf, n, 0U };
    return r;
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
    return ( uint16_t ) ( ( ( uint16_t ) hi << 8 ) | lo );
}

static inline uint32_t FR_next_u32( FR_Reader * r )
{
    uint32_t lo = FR_next_u16( r );
    uint32_t hi = FR_next_u16( r );
    return ( hi << 16 ) | lo;
}

static inline int32_t FR_next_s32( FR_Reader * r )
{
    return ( int32_t ) FR_next_u32( r );
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

static inline size_t FR_next_string( FR_Reader * r, char * out, size_t max_len )
{
    if( ( out == NULL ) || ( max_len == 0U ) )
    {
        return 0U;
    }

    out[ 0 ] = '\0';

    if( max_len == 1U )
    {
        return 0U;
    }

    size_t max_copy = max_len - 1U;
    size_t rem = FR_remaining( r );

    if( ( rem == 0U ) || ( max_copy == 0U ) )
    {
        return 0U;
    }

    size_t span = ( rem < max_copy ) ? rem : max_copy;
    size_t len = span ? ( ( size_t ) FR_next_u16( r ) % ( span + 1U ) ) : 0U;

    if( len == 0U )
    {
        return 0U;
    }

    size_t got = FR_next_bytes( r, ( unsigned char * ) out, len );
    out[ got ] = '\0';
    return got;
}

// ====================================================================
// Event group under test + helper task
// ====================================================================
static StaticEventGroup_t g_event_group_buf;
static EventGroupHandle_t g_event_group = NULL;

#define HELPER_TASK_STACK_SIZE   ( configMINIMAL_STACK_SIZE )
#define HELPER_TASK_PRIORITY     ( tskIDLE_PRIORITY + 1U )

static StackType_t g_helper_task_stack[ HELPER_TASK_STACK_SIZE ];
static StaticTask_t g_helper_task_tcb;
static TaskHandle_t g_helper_task_handle = NULL;

typedef struct HelperControl
{
    EventBits_t bits_to_set;
    EventBits_t bits_to_clear;
    TickType_t delay_ticks;
    uint8_t enable;
} HelperControl_t;

static HelperControl_t g_helper_ctrl;

static void helper_task( void * pvParameters )
{
    ( void ) pvParameters;

    for( ;; )
    {
        if( g_helper_ctrl.enable == 0U )
        {
            vTaskSuspend( NULL );
            continue;
        }

        if( g_event_group != NULL )
        {
            if( g_helper_ctrl.bits_to_set != 0U )
            {
                ( void ) xEventGroupSetBits( g_event_group, g_helper_ctrl.bits_to_set );
            }

            if( g_helper_ctrl.bits_to_clear != 0U )
            {
                ( void ) xEventGroupClearBits( g_event_group, g_helper_ctrl.bits_to_clear );
            }
        }

        vTaskDelay( g_helper_ctrl.delay_ticks );
    }
}

static void ensure_helper_task_created( void )
{
    if( g_helper_task_handle == NULL )
    {
        g_helper_task_handle = xTaskCreateStatic( helper_task,
                                                  "egHelp",
                                                  HELPER_TASK_STACK_SIZE,
                                                  NULL,
                                                  HELPER_TASK_PRIORITY,
                                                  g_helper_task_stack,
                                                  &g_helper_task_tcb );
        configASSERT( g_helper_task_handle != NULL );
    }
}

static void configure_helper_from_fuzz( uint8_t pattern,
                                        EventBits_t bits_a,
                                        EventBits_t bits_b,
                                        TickType_t delay_ticks )
{
    g_helper_ctrl.enable = ( uint8_t ) ( pattern & 0x01U );

    if( ( pattern & 0x02U ) != 0U )
    {
        g_helper_ctrl.bits_to_set = bits_a;
    }
    else
    {
        g_helper_ctrl.bits_to_set = bits_b;
    }

    if( ( pattern & 0x04U ) != 0U )
    {
        g_helper_ctrl.bits_to_clear = bits_b;
    }
    else
    {
        g_helper_ctrl.bits_to_clear = bits_a;
    }

    if( delay_ticks == 0U )
    {
        delay_ticks = 1U;
    }

    g_helper_ctrl.delay_ticks = delay_ticks;

    if( g_helper_task_handle != NULL )
    {
        vTaskResume( g_helper_task_handle );
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
        /* Ensure we have enough fuzz data for this iteration: we will consume at
         * least 1 (control) + 4 (bits_to_set) + 4 (bits_to_clear) + 4 (wait_bits)
         * + 1 (wait_flags) + 2 (wait_timeout) = 16 bytes. */
        if( FR_remaining( &fr ) < 16U )
        {
            break;
        }

        /* Lazily create the event group once. */
        if( g_event_group == NULL )
        {
            g_event_group = xEventGroupCreateStatic( &g_event_group_buf );
            configASSERT( g_event_group != NULL );
            ensure_helper_task_created();
        }

        /* Derive parameters from fuzz input. */
        uint8_t ctrl = FR_next_u8( &fr );
        uint32_t raw_set = FR_next_u32( &fr );
        uint32_t raw_clear = FR_next_u32( &fr );
        uint32_t raw_wait_bits = FR_next_u32( &fr );
        uint8_t wait_flags = FR_next_u8( &fr );
        uint16_t wait_timeout_raw = FR_next_u16( &fr );

        /* Mask out control bits reserved by the kernel. */
        EventBits_t bits_to_set = ( EventBits_t ) ( raw_set & ~eventEVENT_BITS_CONTROL_BYTES );
        EventBits_t bits_to_clear = ( EventBits_t ) ( raw_clear & ~eventEVENT_BITS_CONTROL_BYTES );
        EventBits_t wait_bits = ( EventBits_t ) ( raw_wait_bits & ~eventEVENT_BITS_CONTROL_BYTES );

        /* Map timeout into a small bounded range to avoid long blocking. */
        TickType_t wait_timeout = ( TickType_t ) ( wait_timeout_raw % 10U );

        /* Configure helper task behavior based on fuzz data. */
        configure_helper_from_fuzz( ctrl, bits_to_set, bits_to_clear, wait_timeout + 1U );

        /* Optionally pre-set some bits before clearing. */
        if( ( ctrl & 0x80U ) != 0U )
        {
            ( void ) xEventGroupSetBits( g_event_group, bits_to_set );
        }

        /* Optionally perform a wait on bits to introduce blocking/unblocking
         * paths. */
        if( wait_bits != 0U )
        {
            BaseType_t wait_all = ( ( wait_flags & 0x01U ) != 0U ) ? pdTRUE : pdFALSE;
            BaseType_t clear_on_exit = ( ( wait_flags & 0x02U ) != 0U ) ? pdTRUE : pdFALSE;

            ( void ) xEventGroupWaitBits( g_event_group,
                                          wait_bits,
                                          clear_on_exit,
                                          wait_all,
                                          wait_timeout );
        }

        /* Now exercise xEventGroupClearBits with fuzz-derived mask. Ensure we
         * never pass control bits to satisfy configASSERT. */
        EventBits_t mask_for_clear;

        if( ( ctrl & 0x40U ) != 0U )
        {
            mask_for_clear = bits_to_clear;
        }
        else
        {
            mask_for_clear = bits_to_set ^ bits_to_clear;
            mask_for_clear &= ~eventEVENT_BITS_CONTROL_BYTES;
        }

        /* Capture current bits via a get to compare with return value. */
        EventBits_t before = xEventGroupGetBits( g_event_group );

        EventBits_t ret = xEventGroupClearBits( g_event_group, mask_for_clear );

        /* Basic consistency checks: return value should reflect the state before
         * clearing, and the bits indicated by mask_for_clear should now be
         * clear. */
        configASSERT( ( before & mask_for_clear ) == ( ret & mask_for_clear ) );

        EventBits_t after = xEventGroupGetBits( g_event_group );
        configASSERT( ( after & mask_for_clear ) == 0U );

        /* Optionally re-set some bits after clearing to create varied state for
         * the next iteration. */
        if( ( ctrl & 0x20U ) != 0U )
        {
            ( void ) xEventGroupSetBits( g_event_group,
                                         ( EventBits_t ) ( ~mask_for_clear &
                                                           ~eventEVENT_BITS_CONTROL_BYTES ) );
        }
    }

    printf( "[TEST_CASE_COMPLETED]\n" );
    fflush( stdout );

    BREAKPOINT();
}

// FreeRTOS 任务包装器
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
