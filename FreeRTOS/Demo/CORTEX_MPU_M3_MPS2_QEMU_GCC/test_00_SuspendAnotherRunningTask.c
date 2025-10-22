/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: SuspendAnotherRunningTask
 * 生成时间: 2025-10-17 15:21:41
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: SuspendAnotherRunningTask
 * 描述: A lower-priority task suspends a higher-priority task. It verifies that the suspended task stops executing and correctly resumes later. This tests the basic functionality of suspending a task other than the caller.
 * 
 * 生成上下文:
 * 检测到的API函数: ALT_CAN_MSGHAND_MOIPA_INTPND_25_SET, DCAR_BASE           , configMAX_TASK_NAME_LEN                  , TAMP_SMISR_TAMP6MF_Pos       , UNITY_TEST_ASSERT_NOT_EQUAL_HEX8, RCC_AHB4LPENR_GPIOJLPEN_Msk            , ADC_SQR5_SQ2_2                       , SMB1_CONFIGURATION_ENIDI_Pos          , IWDG_FLAG_RVU                   , UICR_KEYSLOT_KEY_VALUE_VALUE_Msk 
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


// LibAFL集成必需的全局变量和函数
#define MAX_FUZZ_INPUT_SIZE 128
__attribute__((used, visibility("default"))) unsigned char FUZZ_INPUT[MAX_FUZZ_INPUT_SIZE];

// LibAFL断点函数
int __attribute__((noinline, used, visibility("default"))) BREAKPOINT(void)
{
    // LibAFL会在这里设置断点
    return 0;
}

// ====================================================================
// 测试用例辅助函数和全局变量
// ====================================================================
static TaskHandle_t xWorkerTaskHandle = NULL;
static volatile uint32_t ulWorkerTaskCounter = 0;

static void vWorkerTask(void *pvParameters)
{
    (void)pvParameters;
    for(;;)
    {
        ulWorkerTaskCounter++;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ====================================================================
// 主测试函数
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void)
{
    // 测试用例: SuspendAnotherRunningTask
    // API类别: task_management
    // 描述: A lower-priority task suspends a higher-priority task. It verifies that the suspended task stops executing and correctly resumes later. This tests the basic functionality of suspending a task other than the caller.
    
    TaskHandle_t xCurrentTaskHandle = xTaskGetCurrentTaskHandle();
    UBaseType_t uxOriginalPriority = uxTaskPriorityGet(NULL);

    ulWorkerTaskCounter = 0;
    xWorkerTaskHandle = NULL;

    /* Create a worker task with a higher priority than the current task */
    xTaskCreate(vWorkerTask, "Worker", configMINIMAL_STACK_SIZE, NULL, uxOriginalPriority + 1, &xWorkerTaskHandle);

    /* Ensure the worker task handle is valid */
    if (xWorkerTaskHandle == NULL) { return; }

    /* Yield to allow the higher-priority worker task to run and increment its counter */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* At this point, the counter should be non-zero */
    if (ulWorkerTaskCounter == 0) { vTaskDelete(xWorkerTaskHandle); return; }

    /* Suspend the worker task */
    vTaskSuspend(xWorkerTaskHandle);

    /* Verify task state is suspended */
    eTaskState eState = eTaskGetState(xWorkerTaskHandle);
    if (eState != eSuspended) { vTaskDelete(xWorkerTaskHandle); return; }

    /* Store the counter value */
    uint32_t ulCounterWhenSuspended = ulWorkerTaskCounter;

    /* Delay for a while. The worker task should not run. */
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Verify the counter has not changed */
    if (ulWorkerTaskCounter != ulCounterWhenSuspended) { vTaskDelete(xWorkerTaskHandle); return; }

    /* Resume the worker task */
    vTaskResume(xWorkerTaskHandle);

    /* Delay to let the worker task run again */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Verify the counter has increased */
    if (ulWorkerTaskCounter <= ulCounterWhenSuspended) { vTaskDelete(xWorkerTaskHandle); return; }

    /* Clean up */
    vTaskDelete(xWorkerTaskHandle);
    xWorkerTaskHandle = NULL;
    
    // 调用BREAKPOINT函数结束本次fuzzing
    BREAKPOINT();
}


// FreeRTOS任务包装器
void fuzz_task(void)
{
    // 创建模糊测试任务
    xTaskCreate(
        (TaskFunction_t)test_task,
        "FuzzTask",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );
    
    // 启动调度器
    vTaskStartScheduler();
    
    // 不应该到达这里
    for (;;) {
        // 空循环
    }
}


