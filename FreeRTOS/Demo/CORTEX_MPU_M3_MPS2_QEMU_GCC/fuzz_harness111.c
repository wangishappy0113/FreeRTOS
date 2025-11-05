/*
 * FreeRTOS queue fuzz harness example
 *
 * This harness targets the xQueueSend/xQueueReceive APIs under the MPU demo
 * configuration. It honours all template/prompt constraints used by the
 * fuzzing pipeline: static allocation, privileged supervision, configASSERT
 * checks, multi-byte FUZZ_INPUT usage, and non-returning task functions.
 */

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "queue.h"
#include "task.h"
#include <stdio.h>

#define MAX_FUZZ_INPUT_SIZE    128U
__attribute__((used, visibility("default"))) unsigned char FUZZ_INPUT[MAX_FUZZ_INPUT_SIZE];

int __attribute__((noinline, used, visibility("default"))) BREAKPOINT(void)
{
    return 0;
}

#define FUZZ_TASK_STACK_DEPTH      (configMINIMAL_STACK_SIZE * 4U)
static StackType_t xFuzzTaskStack[FUZZ_TASK_STACK_DEPTH];
static StaticTask_t xFuzzTaskTCB;
#define FUZZ_TASK_PRIORITY         ((tskIDLE_PRIORITY + 2U) | portPRIVILEGE_BIT)

#define PRODUCER_STACK_DEPTH       (configMINIMAL_STACK_SIZE + 128U)
#define CONSUMER_STACK_DEPTH       (configMINIMAL_STACK_SIZE + 128U)

static StackType_t xProducerStack[PRODUCER_STACK_DEPTH];
static StaticTask_t xProducerTCB;
static TaskHandle_t xProducerHandle = NULL;

static StackType_t xConsumerStack[CONSUMER_STACK_DEPTH];
static StaticTask_t xConsumerTCB;
static TaskHandle_t xConsumerHandle = NULL;

#define QUEUE_LENGTH   3U
#define QUEUE_ITEM_SIZE sizeof(uint32_t)

static StaticQueue_t xQueueBuffer;
static uint8_t ucQueueStorage[QUEUE_LENGTH * QUEUE_ITEM_SIZE];
static QueueHandle_t xTestQueue = NULL;

static volatile BaseType_t xQueueFilled = pdFALSE;
static volatile BaseType_t xHarnessComplete = pdFALSE;
static volatile BaseType_t xConsumerDone = pdFALSE;
static volatile uint32_t ulExpectedSequence[QUEUE_LENGTH];
static volatile uint32_t ulReceivedSequence[QUEUE_LENGTH];

static void prvQueueProducerTask(void *pvParameters)
{
    (void) pvParameters;

    const TickType_t xSendBlockTime = pdMS_TO_TICKS((TickType_t)((FUZZ_INPUT[3] & 0x0FU) + 1U));
    uint32_t ulValue = 0U;

    configASSERT(xTestQueue != NULL);

    ulExpectedSequence[0] = ((uint32_t) FUZZ_INPUT[0] << 1) | 0x01U;
    ulExpectedSequence[1] = ((uint32_t) FUZZ_INPUT[1] << 1) | 0x02U;
    ulExpectedSequence[2] = ((uint32_t) FUZZ_INPUT[2] << 1) | 0x03U;

    ulValue = ulExpectedSequence[0];
    configASSERT(xQueueSend(xTestQueue, &ulValue, xSendBlockTime) == pdTRUE);

    ulValue = ulExpectedSequence[1];
    configASSERT(xQueueSend(xTestQueue, &ulValue, xSendBlockTime) == pdTRUE);

    ulValue = ulExpectedSequence[2];
    configASSERT(xQueueSend(xTestQueue, &ulValue, xSendBlockTime) == pdTRUE);

    ulValue = 0xDEADBEEFU;
    configASSERT(xQueueSend(xTestQueue, &ulValue, 0U) == errQUEUE_FULL);

    xQueueFilled = pdTRUE;
    configASSERT(xConsumerHandle != NULL);
    (void) xTaskNotifyGive(xConsumerHandle);

    while (xConsumerDone == pdFALSE)
    {
        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    vTaskDelete(NULL);

    for (;;)
    {
        /* Should never execute. */
    }
}

static void prvQueueConsumerTask(void *pvParameters)
{
    (void) pvParameters;

    const TickType_t xReceiveBlockTime = pdMS_TO_TICKS((TickType_t)((FUZZ_INPUT[4] & 0x0FU) + 1U));
    uint32_t ulValue = 0U;
    UBaseType_t uxIndex = 0U;

    configASSERT(xTestQueue != NULL);

    (void) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    configASSERT(xQueueFilled == pdTRUE);

    for (uxIndex = 0U; uxIndex < QUEUE_LENGTH; uxIndex++)
    {
        configASSERT(xQueueReceive(xTestQueue, &ulValue, xReceiveBlockTime) == pdTRUE);
        ulReceivedSequence[uxIndex] = ulValue;
        configASSERT(ulReceivedSequence[uxIndex] == ulExpectedSequence[uxIndex]);
    }

    configASSERT(uxQueueMessagesWaiting(xTestQueue) == 0U);

    xConsumerDone = pdTRUE;
    xHarnessComplete = pdTRUE;

    vTaskDelete(NULL);

    for (;;)
    {
        /* Should never execute. */
    }
}

void __attribute__((used, visibility("default"))) test_task(void)
{
    const UBaseType_t uxProducerPriority = (tskIDLE_PRIORITY + 2U) | portPRIVILEGE_BIT;
    const UBaseType_t uxConsumerPriority = (tskIDLE_PRIORITY + 1U) | portPRIVILEGE_BIT;

    configASSERT(uxProducerPriority < configMAX_PRIORITIES);

    xQueueFilled = pdFALSE;
    xHarnessComplete = pdFALSE;
    xConsumerDone = pdFALSE;

    xTestQueue = xQueueCreateStatic(
        QUEUE_LENGTH,
        QUEUE_ITEM_SIZE,
        ucQueueStorage,
        &xQueueBuffer);
    configASSERT(xTestQueue != NULL);

    xConsumerHandle = xTaskCreateStatic(
        prvQueueConsumerTask,
        "ConsQ",
        CONSUMER_STACK_DEPTH,
        NULL,
        uxConsumerPriority,
        xConsumerStack,
        &xConsumerTCB);
    configASSERT(xConsumerHandle != NULL);

    xProducerHandle = xTaskCreateStatic(
        prvQueueProducerTask,
        "ProdQ",
        PRODUCER_STACK_DEPTH,
        NULL,
        uxProducerPriority,
        xProducerStack,
        &xProducerTCB);
    configASSERT(xProducerHandle != NULL);

    while (xHarnessComplete == pdFALSE)
    {
        vTaskDelay(pdMS_TO_TICKS(1U));
    }

    BREAKPOINT();

    vTaskDelete(NULL);

    for (;;)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

void fuzz_task(void)
{
    TaskHandle_t xHandle = xTaskCreateStatic(
        test_task,
        "QueueHarness",
        FUZZ_TASK_STACK_DEPTH,
        NULL,
        FUZZ_TASK_PRIORITY,
        xFuzzTaskStack,
        &xFuzzTaskTCB);

    if (xHandle == NULL)
    {
        printf("Harness task creation failed\n");
        fflush(stdout);
        configASSERT(0);
    }

    vTaskStartScheduler();

    for (;;)
    {
        /* Scheduler should never return. */
    }
}
