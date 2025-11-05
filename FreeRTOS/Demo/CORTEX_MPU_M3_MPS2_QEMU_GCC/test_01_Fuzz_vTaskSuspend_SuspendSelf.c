/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: Fuzz_vTaskSuspend_SuspendSelf
 * 生成时间: 2025-10-24 15:37:46
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: Fuzz_vTaskSuspend_SuspendSelf
 * 描述: A privileged task creates a target task which, after a fuzzed delay, suspends itself by calling vTaskSuspend(NULL). The creator task then verifies the target's suspended state, resumes it, and waits for it to self-delete.
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
#define SELF_SUSPEND_TASK_STACK_SIZE 256
static StackType_t uxSelfSuspendStack[SELF_SUSPEND_TASK_STACK_SIZE];
static StaticTask_t xSelfSuspendTCB;

/* Pass the creator's handle to the new task for signaling. */
struct TaskParams {
    TaskHandle_t xCreatorHandle;
    uint8_t ucDelay;
};

void vSelfSuspendingTask(void *pvParameters) {
    struct TaskParams *pxParams = (struct TaskParams *)pvParameters;
    TaskHandle_t xCreator = pxParams->xCreatorHandle;
    uint8_t ucDelayBeforeSuspend = pxParams->ucDelay;

    /* Optional delay before self-suspension based on fuzz input. */
    if (ucDelayBeforeSuspend > 0) {
        vTaskDelay(pdMS_TO_TICKS(ucDelayBeforeSuspend));
    }

    /* Signal creator right before suspending. */
    (void)xTaskNotifyGive(xCreator);

    vTaskSuspend(NULL);

    /* Execution should only resume here after being resumed by the creator. */
    /* Signal again to confirm resumption. */
    (void)xTaskNotifyGive(xCreator);

    /* Self-delete for cleanup. */
    vTaskDelete(NULL);
}

// ====================================================================
// 主测试函数
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void)
{
    // 测试用例: Fuzz_vTaskSuspend_SuspendSelf
    // API类别: task_management
    // 描述: A privileged task creates a target task which, after a fuzzed delay, suspends itself by calling vTaskSuspend(NULL). The creator task then verifies the target's suspended state, resumes it, and waits for it to self-delete.
    
    TaskHandle_t xSelfSuspendHandle;
    struct TaskParams xParams;

    /* Use FUZZ_INPUT to control the delay within the target task. */
    xParams.ucDelay = (FUZZ_INPUT_LEN > 0) ? (FUZZ_INPUT[0] % 100) : 10;
    xParams.xCreatorHandle = xTaskGetCurrentTaskHandle();

    xSelfSuspendHandle = xTaskCreateStatic(vSelfSuspendingTask, "SelfSusp", SELF_SUSPEND_TASK_STACK_SIZE, &xParams, tskIDLE_PRIORITY + 2, uxSelfSuspendStack, &xSelfSuspendTCB);
    TEST_ASSERT_NOT_NULL(xSelfSuspendHandle);

    /* Wait for the notification indicating the task is about to suspend. */
    TEST_ASSERT_EQUAL(pdTRUE, ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200)));

    /* Give a moment for the suspend to actually happen. */
    vTaskDelay(pdMS_TO_TICKS(10)); 

    /* Check the task's state. */
    eTaskState eState = eTaskGetState(xSelfSuspendHandle);
    TEST_ASSERT_EQUAL(eSuspended, eState);

    /* Resume the task. */
    vTaskResume(xSelfSuspendHandle);

    /* Wait for the second notification indicating the task has resumed and is about to delete itself. */
    TEST_ASSERT_EQUAL(pdTRUE, ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50)));

    /* The task should now be gone. A delay to allow the idle task to clean up. */
    vTaskDelay(pdMS_TO_TICKS(10));
    eState = eTaskGetState(xSelfSuspendHandle);
    TEST_ASSERT_EQUAL(eDeleted, eState);
    
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


