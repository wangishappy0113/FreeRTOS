/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: Fuzz_vTaskSuspend_SuspendTaskBlockedOnNotification
 * 生成时间: 2025-10-24 15:37:46
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: Fuzz_vTaskSuspend_SuspendTaskBlockedOnNotification
 * 描述: Tests suspending a task that is currently blocked waiting for a task notification. This validates that the task's notification state is correctly reset upon suspension, as per the source code logic. Fuzz input adds a variable delay.
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
#define BLOCKED_TASK_STACK_SIZE 256
static StackType_t uxBlockedTaskStack[BLOCKED_TASK_STACK_SIZE];
static StaticTask_t xBlockedTaskTCB;

static volatile BaseType_t xBlockedTaskResumed = pdFALSE;

void vBlockedTask(void *pvParameters) {
    (void)pvParameters;
    uint32_t ulNotificationValue;

    /* Block indefinitely waiting for a notification. */
    xTaskNotifyWait(0x00, 0xFFFFFFFF, &ulNotificationValue, portMAX_DELAY);

    /* This part should only be reached after being suspended and then resumed. */
    xBlockedTaskResumed = pdTRUE;

    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ====================================================================
// 主测试函数
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void)
{
    // 测试用例: Fuzz_vTaskSuspend_SuspendTaskBlockedOnNotification
    // API类别: task_management
    // 描述: Tests suspending a task that is currently blocked waiting for a task notification. This validates that the task's notification state is correctly reset upon suspension, as per the source code logic. Fuzz input adds a variable delay.
    
    TaskHandle_t xBlockedTaskHandle;

    xBlockedTaskHandle = xTaskCreateStatic(vBlockedTask, "Blocked", BLOCKED_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, uxBlockedTaskStack, &xBlockedTaskTCB);
    TEST_ASSERT_NOT_NULL(xBlockedTaskHandle);

    /* Allow the task to run and block on xTaskNotifyWait. */
    vTaskDelay(pdMS_TO_TICKS(20));

    /* Verify it is in the blocked state. */
    TEST_ASSERT_EQUAL(eBlocked, eTaskGetState(xBlockedTaskHandle));

    /* Use fuzz input for a random delay before suspension. */
    if (FUZZ_INPUT_LEN > 0) {
        vTaskDelay(pdMS_TO_TICKS(FUZZ_INPUT[0] % 20));
    }

    /* Suspend the blocked task. */
    vTaskSuspend(xBlockedTaskHandle);

    /* Verify it is now in the suspended state. */
    TEST_ASSERT_EQUAL(eSuspended, eTaskGetState(xBlockedTaskHandle));

    /* Now, send a notification. If the task's state was not reset, this might unblock it. */
    (void)xTaskNotify(xBlockedTaskHandle, 0x123, eSetValueWithOverwrite);

    /* The task should remain suspended. */
    vTaskDelay(pdMS_TO_TICKS(20));
    TEST_ASSERT_EQUAL(eSuspended, eTaskGetState(xBlockedTaskHandle));

    /* Resume the task. It should now be in the ready state, not blocked, and its wait should have been cancelled. */
    vTaskResume(xBlockedTaskHandle);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Check if it's now ready or running. */
    eTaskState eState = eTaskGetState(xBlockedTaskHandle);
    TEST_ASSERT_TRUE(eState == eReady || eState == eRunning);

    /* Verify that the code after xTaskNotifyWait has executed. */
    TEST_ASSERT_EQUAL(pdTRUE, xBlockedTaskResumed);

    /* Clean up. */
    vTaskDelete(xBlockedTaskHandle);
    
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


