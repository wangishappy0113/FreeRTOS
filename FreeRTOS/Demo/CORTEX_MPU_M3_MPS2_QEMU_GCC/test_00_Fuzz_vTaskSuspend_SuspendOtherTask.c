/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: Fuzz_vTaskSuspend_SuspendOtherTask
 * 生成时间: 2025-10-24 15:37:46
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: Fuzz_vTaskSuspend_SuspendOtherTask
 * 描述: A privileged task creates and then suspends another lower-priority task. It then verifies the task's state, resumes it, and cleans up. Fuzz input controls a delay before suspension.
 * 
 * 生成上下文:
 * 检测到的API函数: SYSCFG_CFGR_SRAM2L_Pos          , LL_USART_ConfigIrdaMode, SDRAMC_CR_NC_Msk , ACC_DisableChannel, FMC_NORSRAM_BANK2                       , tmrCOMMAND_STOP                         , AT91C_SUPC_GPBRON     , LL_SPI_PHASE_1EDGE                         , RTC_SetAlarm, ETH_MACMDIOAR_CR_DIV14AR_Pos                  
 * 主要文件: /home/zwz/FreeRTOS/FreeRTOS/Test/CMock/CMock/examples/make_example/src/main.c, /home/zwz/FreeRTOS/FreeRTOS/Test/Target/boards/pico/main.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/lwIP_Demo_Rowley_ARM7/main.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_M4F_Infineon_XMC4500_GCC_Atollic/src/main.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/msp430_IAR/main.c
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


#include <stdio.h>


// LibAFL集成必需的全局变量和函数
#define MAX_FUZZ_INPUT_SIZE 128
__attribute__((used, visibility("default"))) unsigned char FUZZ_INPUT[MAX_FUZZ_INPUT_SIZE];

// LibAFL断点函数
int __attribute__((noinline, used, visibility("default"))) BREAKPOINT(void)
{
    // LibAFL会在这里设置断点
    return 0;
}


#define FUZZ_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 4)
static StackType_t xFuzzTaskStack[FUZZ_TASK_STACK_DEPTH];
static StaticTask_t xFuzzTaskTCB;
#define FUZZ_TASK_PRIORITY ((tskIDLE_PRIORITY + 1) | portPRIVILEGE_BIT)


// ====================================================================
// 测试用例辅助函数和全局变量
// ====================================================================
#define TARGET_TASK_STACK_SIZE 256
static StackType_t uxTargetTaskStack[TARGET_TASK_STACK_SIZE];
static StaticTask_t xTargetTaskTCB;

static volatile BaseType_t xTargetTaskHasRun = pdFALSE;

void vTargetTask(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        xTargetTaskHasRun = pdTRUE;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ====================================================================
// 主测试函数
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void)
{
    // 测试用例: Fuzz_vTaskSuspend_SuspendOtherTask
    // API类别: task_management
    // 描述: A privileged task creates and then suspends another lower-priority task. It then verifies the task's state, resumes it, and cleans up. Fuzz input controls a delay before suspension.
    
    TaskHandle_t xTargetTaskHandle;
    UBaseType_t uxInitialPriority = tskIDLE_PRIORITY + 1;

    /* Create the target task with privileged access bit, though not strictly needed here. */
    xTargetTaskHandle = xTaskCreateStatic(vTargetTask, "Target", TARGET_TASK_STACK_SIZE, NULL, uxInitialPriority | portPRIVILEGE_BIT, uxTargetTaskStack, &xTargetTaskTCB);
    TEST_ASSERT_NOT_NULL(xTargetTaskHandle);

    /* Let the target task run at least once. */
    vTaskDelay(pdMS_TO_TICKS(20));
    TEST_ASSERT_EQUAL(pdTRUE, xTargetTaskHasRun);

    /* Introduce a fuzzed delay before suspending. */
    if (FUZZ_INPUT_LEN > 0) {
        uint8_t delay_ms = FUZZ_INPUT[0] % 50; /* Bounded delay 0-49ms */
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    /* Suspend the target task. */
    vTaskSuspend(xTargetTaskHandle);

    /* Verify the task is in the suspended state. */
    eTaskState eState = eTaskGetState(xTargetTaskHandle);
    TEST_ASSERT_EQUAL(eSuspended, eState);

    /* Reset the flag and wait to see if it changes (it shouldn't). */
    xTargetTaskHasRun = pdFALSE;
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(pdFALSE, xTargetTaskHasRun);

    /* Resume the task to allow for cleanup. */
    vTaskResume(xTargetTaskHandle);

    /* Wait for it to run again to confirm it resumed. */
    vTaskDelay(pdMS_TO_TICKS(20));
    TEST_ASSERT_EQUAL(pdTRUE, xTargetTaskHasRun);

    /* Clean up the task. */
    vTaskDelete(xTargetTaskHandle);
    
    // 调用BREAKPOINT函数结束本次fuzzing
    BREAKPOINT();
}


// FreeRTOS任务包装器
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


