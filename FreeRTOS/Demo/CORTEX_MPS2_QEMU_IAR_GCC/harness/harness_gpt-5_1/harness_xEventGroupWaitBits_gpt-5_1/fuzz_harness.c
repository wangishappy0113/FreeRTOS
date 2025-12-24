/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: xEventGroupWaitBits_basic_fuzz
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: event_groups
 */

/*
 * 测试用例详细信息:
 * 名称: xEventGroupWaitBits_basic_fuzz
 * 描述: Fuzzes xEventGroupWaitBits with varying bit masks, clear/wait modes, and timeouts while another task sets/clears bits based on fuzz input.
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
    for (;;)
    {
        __asm volatile("nop");
    }
}

#define FUZZ_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 4)
static StackType_t xFuzzTaskStack[FUZZ_TASK_STACK_DEPTH];
static StaticTask_t xFuzzTaskTCB;
#define FUZZ_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

// ====================================================================
// Fuzz Reader 实现（FR_* 前缀）
// ====================================================================
typedef struct
{
    const unsigned char *data;
    size_t size;
    size_t off;
} FR_Reader;

#define FR_init_SELECT(_1,_2,NAME,...) NAME
#define FR_init(...) FR_init_SELECT(__VA_ARGS__, FR_init_2, FR_init_1)(__VA_ARGS__)

static inline FR_Reader FR_init_1(const unsigned char *buf)
{
    FR_Reader r = { buf, MAX_FUZZ_INPUT_SIZE, 0 };
    return r;
}

static inline FR_Reader FR_init_2(const unsigned char *buf, size_t n)
{
    FR_Reader r = { buf, n, 0 };
    return r;
}

static inline size_t FR_remaining(FR_Reader *r)
{
    return (r->off < r->size) ? (r->size - r->off) : 0;
}

static inline uint8_t FR_next_u8(FR_Reader *r)
{
    if (r->off + 1 <= r->size)
    {
        return r->data[r->off++];
    }
    return 0;
}

static inline uint16_t FR_next_u16(FR_Reader *r)
{
    uint16_t lo = FR_next_u8(r);
    uint16_t hi = FR_next_u8(r);
    return (uint16_t)((hi << 8) | lo);
}

static inline uint32_t FR_next_u32(FR_Reader *r)
{
    uint32_t lo = FR_next_u16(r);
    uint32_t hi = FR_next_u16(r);
    return (hi << 16) | lo;
}

static inline int32_t FR_next_s32(FR_Reader *r)
{
    return (int32_t)FR_next_u32(r);
}

static inline uint32_t FR_next_range(FR_Reader *r, uint32_t min_v, uint32_t max_v)
{
    if (max_v <= min_v)
    {
        return min_v;
    }
    uint32_t span = max_v - min_v + 1u;
    return min_v + (FR_next_u32(r) % span);
}

static inline size_t FR_next_bytes(FR_Reader *r, unsigned char *out, size_t n)
{
    size_t rem = FR_remaining(r);
    if (n > rem)
    {
        n = rem;
    }
    if (n != 0u)
    {
        memcpy(out, r->data + r->off, n);
        r->off += n;
    }
    return n;
}

static inline size_t FR_next_string(FR_Reader *r, char *out, size_t max_len)
{
    if ((out == NULL) || (max_len == 0u))
    {
        return 0;
    }

    out[0] = '\0';

    if (max_len == 1u)
    {
        return 0;
    }

    size_t max_copy = max_len - 1u;
    size_t rem = FR_remaining(r);

    if ((rem == 0u) || (max_copy == 0u))
    {
        return 0;
    }

    size_t span = (rem < max_copy) ? rem : max_copy;
    size_t len = span ? (FR_next_u16(r) % (span + 1u)) : 0u;

    if (len == 0u)
    {
        return 0;
    }

    size_t got = FR_next_bytes(r, (unsigned char *) out, len);
    out[got] = '\0';
    return got;
}

// ====================================================================
// 事件组 fuzz 辅助：静态事件组和辅助任务
// ====================================================================
static StaticEventGroup_t g_event_group_buf;
static EventGroupHandle_t g_event_group = NULL;

#define HELPER_TASK_STACK_SIZE   128
#define HELPER_TASK_PRIORITY     ( tskIDLE_PRIORITY + 1 )

static StackType_t g_helper_stack[ HELPER_TASK_STACK_SIZE ];
static StaticTask_t g_helper_tcb;
static TaskHandle_t g_helper_handle = NULL;

static void helper_event_task( void *pvParameters )
{
    ( void ) pvParameters;

    // 使用局部 Reader，从全局 FUZZ_INPUT 中继续消费剩余字节
    FR_Reader *shared = (FR_Reader *) pvParameters;

    for( int i = 0; i < 4; i++ )
    {
        if( shared == NULL )
        {
            break;
        }

        if( FR_remaining( shared ) < 3u )
        {
            break;
        }

        uint8_t op_byte      = FR_next_u8( shared );
        uint8_t bit_sel      = FR_next_u8( shared );
        uint8_t delay_raw    = FR_next_u8( shared );

        TickType_t delay_ticks = ( TickType_t )( delay_raw & 0x0Fu );
        EventBits_t bit_mask   = ( EventBits_t )( 1u << ( bit_sel & 0x07u ) );

        if( delay_ticks > 0u )
        {
            vTaskDelay( delay_ticks );
        }

        if( g_event_group != NULL )
        {
            if( ( op_byte & 0x01u ) != 0u )
            {
                ( void ) xEventGroupSetBits( g_event_group, bit_mask );
            }
            else
            {
                ( void ) xEventGroupClearBits( g_event_group, bit_mask );
            }
        }
    }

    g_helper_handle = NULL;
    vTaskDelete( NULL );
}

static void ensure_event_group_created( void )
{
    if( g_event_group == NULL )
    {
        g_event_group = xEventGroupCreateStatic( &g_event_group_buf );
        configASSERT( g_event_group != NULL );
    }
}

static void maybe_start_helper_task( FR_Reader *shared_reader )
{
    if( ( g_helper_handle == NULL ) && ( shared_reader != NULL ) )
    {
        TaskHandle_t h = xTaskCreateStatic( helper_event_task,
                                            "evhlp",
                                            HELPER_TASK_STACK_SIZE,
                                            ( void * ) shared_reader,
                                            HELPER_TASK_PRIORITY,
                                            g_helper_stack,
                                            &g_helper_tcb );
        if( h != NULL )
        {
            g_helper_handle = h;
        }
    }
}

// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
static void test_task( void *pvParameters )
{
    ( void ) pvParameters;

    FR_Reader fr = FR_init( FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE );

    unsigned char fr_baseline[16] = { 0 };
    (void) FR_next_bytes( &fr, fr_baseline, sizeof( fr_baseline ) );

    unsigned int iterations = (unsigned int) FR_next_range( &fr, 0u, 10u );

    if( xTaskGetSchedulerState() != taskSCHEDULER_RUNNING )
    {
        printf( "[ERROR] Scheduler not running - aborting test_task\n" );
        fflush( stdout );
        vTaskDelete( NULL );
    }

    for( unsigned int i = 0; i < iterations; i++ )
    {
        ensure_event_group_created();

        if( FR_remaining( &fr ) < 6u )
        {
            break;
        }

        maybe_start_helper_task( &fr );

        uint8_t  mode         = FR_next_u8( &fr );
        uint16_t raw_bits     = FR_next_u16( &fr );
        uint8_t  clear_flag   = FR_next_u8( &fr );
        uint8_t  wait_all_flag= FR_next_u8( &fr );
        uint8_t  ticks_sel    = FR_next_u8( &fr );

        EventBits_t uxBitsToWaitFor = ( EventBits_t )( raw_bits & 0x00FFu );
        if( uxBitsToWaitFor == 0u )
        {
            uxBitsToWaitFor = 0x01u;
        }

        BaseType_t xClearOnExit    = ( ( clear_flag    & 0x01u ) != 0u ) ? pdTRUE : pdFALSE;
        BaseType_t xWaitForAllBits = ( ( wait_all_flag & 0x01u ) != 0u ) ? pdTRUE : pdFALSE;

        TickType_t xTicksToWait;
        switch( ticks_sel & 0x03u )
        {
            case 0u: xTicksToWait = ( TickType_t ) 0;  break;
            case 1u: xTicksToWait = ( TickType_t ) 1;  break;
            case 2u: xTicksToWait = ( TickType_t ) 5;  break;
            default: xTicksToWait = ( TickType_t ) 10; break;
        }

        if( ( mode & 0x01u ) != 0u )
        {
            EventBits_t preset = uxBitsToWaitFor;
            if( ( mode & 0x02u ) != 0u )
            {
                preset |= ( EventBits_t )( ( uxBitsToWaitFor << 1 ) & 0x00FFu );
            }
            ( void ) xEventGroupSetBits( g_event_group, preset );
        }
        else
        {
            ( void ) xEventGroupClearBits( g_event_group, uxBitsToWaitFor );
        }

        EventBits_t ret_bits = xEventGroupWaitBits( g_event_group,
                                                    uxBitsToWaitFor,
                                                    xClearOnExit,
                                                    xWaitForAllBits,
                                                    xTicksToWait );

        ( void ) ret_bits;
        configASSERT( ( ret_bits & eventEVENT_BITS_CONTROL_BYTES ) == 0u );
    }

    printf( "[TEST_CASE_COMPLETED]\n" );
    fflush( stdout );

    BREAKPOINT();
}

// FreeRTOS任务包装器
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
