/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Standard includes. */
#include <stdio.h>
#include <string.h>

/* TraceRecorder includes */
#include <trcRecorder.h>

#define mainCREATE_SIMPLE_BLINKY_DEMO_ONLY    1

int __attribute__((noinline)) BREAKPOINT() {
    for (;;) {}
}
/* UART Register Definitions */
#define UART0_ADDRESS                         ( 0x40004000UL )
#define UART0_DATA                            ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 0UL ) ) ) )
#define UART0_STATE                           ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 4UL ) ) ) )
#define UART0_CTRL                            ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 8UL ) ) ) )
#define UART0_BAUDDIV                         ( *( ( ( volatile uint32_t * ) ( UART0_ADDRESS + 16UL ) ) ) )
#define TX_BUFFER_MASK                        ( 1UL )

/* Cortex-M3 System Control Block (用于开启除以零陷阱) */
#define SCB_CCR                               (*(volatile uint32_t *)0xE000ED14)
#define SCB_CCR_DIV_0_TRP_Msk                 (1UL << 4)

extern void main_blinky( void );
extern void main_full( void );

void vFullDemoTickHookFunction( void );
void vFullDemoIdleFunction( void );
static void prvUARTInit( void );

/* ------------------------------------------------------------------- */
/* BUG 函数                                */
/* ------------------------------------------------------------------- */

void divide_by_zero_vulnerability(int divisor) {
    printf("Attempting division: 100 / %d\n", divisor);
    /* 除非开启了 SCB_CCR bit 4，否则这里不会触发异常 */
    volatile int result = 100 / divisor; 
    printf("Division result: %d\n", result);
}

void trigger_stack_overflow() {
    char small_buffer[8]; 
    memset(small_buffer, 'A', 100); 
    strcpy(small_buffer, "This is a very long string that will definitely cause stack overflow");
}

void safe_operation() {
    char buffer[8];
    strncpy(buffer, "Safe", sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    printf("Safe operation completed: %s\n", buffer);
}

/* ------------------------------------------------------------------- */
/* FUZZ HARNESS                             */
/* ------------------------------------------------------------------- */

/* * 1. 加上 __attribute__((used)) 确保符号不被优化
 * 2. 修复了逻辑，确保有路径可以触发 Bug
 * 3. 补全了缺失的括号和返回值
 */
__attribute__((used)) int LLVMFuzzerTestOneInput(unsigned int *Data, unsigned int Size) {
#ifdef TARGET_CUSTOM_INSN
    libafl_qemu_start_phys((void *)Data, Size);
#endif

    if (Size > 1) {
        if (Data[0] % 2 == 0) {
            // 偶数路径
            if(Data[1] % 2 == 0){ 
                // 偶数路径 -> 触发除以零
                printf("Even inputs (%u, %u): Triggering divide zero\n", Data[0], Data[1]);
                divide_by_zero_vulnerability(0);
            }
            else{
                // 奇数路径 -> 触发栈溢出
                printf("Even/Odd inputs (%u, %u): Triggering stack overflow\n", Data[0], Data[1]);
                trigger_stack_overflow();
            }
        } else {
            // 安全路径
            printf("Odd input (%u): Performing safe operation\n", Data[0]);
            safe_operation();
        }
    } else {
        printf("Empty input: Performing safe operation\n");
        safe_operation();
    }

    printf("Harness Finished.\n");


    return BREAKPOINT();

}

/* * 修复了 FUZZ_INPUT，改为 {176, 112, ...}
 * 176 是偶数，112 是偶数 -> 刚好进入 divide_by_zero_vulnerability
 */
unsigned int FUZZ_INPUT[] = {
     176, 112, 175, 19, 255, 255, 255, 127, 18, 0, 0, 255, 255, 1
};

/* ------------------------------------------------------------------- */
/* MAIN                                 */
/* ------------------------------------------------------------------- */

void main( void )
{
    /* 关键：开启 Cortex-M3 除以零异常捕获 */
    SCB_CCR |= SCB_CCR_DIV_0_TRP_Msk;

#if (configUSE_TRACE_FACILITY == 1)
    xTraceInitialize();
    xTraceEnable(TRC_START);
    xTraceTimestampSetPeriod(configCPU_CLOCK_HZ/configTICK_RATE_HZ);
#endif  

    prvUARTInit();

    printf("Hello from FreeRTOS fuzz demo!\r\n");
    
    /* 启动 Fuzzer 测试 (使用全局数组) */
    LLVMFuzzerTestOneInput(FUZZ_INPUT, 50);

    for( ;; ) { }
    
    #if ( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 1 )
    { main_blinky(); }
    #else
    { main_full(); }
    #endif
}

/* ------------------------------------------------------------------- */
/* HOOKS                                  */
/* ------------------------------------------------------------------- */

void vApplicationMallocFailedHook( void ) {
    portDISABLE_INTERRUPTS();
    for( ; ; ) { }
}

void vApplicationIdleHook( void ) { }

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char * pcTaskName ) {
    ( void ) pcTaskName;
    ( void ) pxTask;
    /* 注意：这里也尽量不要 printf，防止 Double Fault */
    portDISABLE_INTERRUPTS();
    for( ; ; ) { }
}

void vApplicationTickHook( void ) {
    #if ( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY != 1 )
    {
        extern void vFullDemoTickHookFunction( void );
        vFullDemoTickHookFunction();
    }
    #endif 
}

void vApplicationDaemonTaskStartupHook( void ) {
     xTraceEnable(TRC_START);
}

void vAssertCalled( const char * pcFileName, uint32_t ulLine ) {
    volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;
    printf( "ASSERT! Line %d, file %s\r\n", ( int ) ulLine, pcFileName );
    taskENTER_CRITICAL();
    while( ulSetToNonZeroInDebuggerToContinue == 0 ) {
        __asm volatile ( "NOP" );
        __asm volatile ( "NOP" );
    }
    taskEXIT_CRITICAL();
}

/* Static Allocation Hooks */
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer, StackType_t ** ppxIdleTaskStackBuffer, uint32_t * pulIdleTaskStackSize ) {
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer, StackType_t ** ppxTimerTaskStackBuffer, uint32_t * pulTimerTaskStackSize ) {
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

static void prvUARTInit( void ) {
    UART0_BAUDDIV = 16;
    UART0_CTRL = 1;
}

int __write( int iFile, char * pcString, int iStringLength ) {
    int iNextChar;
    ( void ) iFile;
    for( iNextChar = 0; iNextChar < iStringLength; iNextChar++ ) {
        while( ( UART0_STATE & TX_BUFFER_MASK ) != 0 ) { }
        UART0_DATA = *pcString;
        pcString++;
    }
    return iStringLength;
}

void * malloc( size_t size ) {
    ( void ) size;
    portDISABLE_INTERRUPTS();
    for( ; ; ) { }
}