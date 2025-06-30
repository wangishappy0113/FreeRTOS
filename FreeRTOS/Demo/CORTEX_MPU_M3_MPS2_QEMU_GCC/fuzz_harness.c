#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>          // For printf, fflush
#include "FreeRTOSConfig.h" // For configMAX_PRIORITIES etc.

// 1. 定义 FUZZ_INPUT (与 fuzzer_breakpoint.rs 中期望的名称一致)
#define MAX_FUZZ_INPUT_SIZE 128 // 与 fuzzer_breakpoint.rs 中的 MAX_INPUT_SIZE 对应或更大
__attribute__((used, visibility("default"))) unsigned char FUZZ_INPUT[MAX_FUZZ_INPUT_SIZE];

// 2. 定义 BREAKPOINT 函数 (与 fuzzer_breakpoint.rs 中期望的名称一致)
int __attribute__((noinline, used, visibility("default"))) BREAKPOINT(void)
{
    // LibAFL QEMU EndCommand 会在此处捕获
    // 无限循环通常就足够了，因为 LibAFL 会通过 PC 匹配来结束
    printf("INTO BREAKPOINT(). \n");
    fflush(stdout);
    volatile int i = 0;
    while (i == 0)
    { /* Infinite loop to signify breakpoint */
    }
    return 0;
}

// 3. 定义 "main" 函数作为 harness 入口 (与 fuzzer_breakpoint.rs 中 main_addr 的期望符号名一致)
// 这个函数将是每个 fuzz case 的实际执行体
void __attribute__((used, visibility("default"))) test_task(void)
{

    // 此函数的内容由 LibAFL 的 StartCommand (在 fuzzer_breakpoint.rs 中配置) 启动
    // LibAFL QEMU executor 会在每个 fuzz case 开始时将 PC 设置到这个函数的地址

    // 示例: 打印收到的输入 (可选, 用于调试)
    // printf("Harness: Received input (first byte: 0x%02x, size: %u)\n", FUZZ_INPUT[0], MAX_FUZZ_INPUT_SIZE);
    // fflush(stdout);

    // ====================================================================
    
    int a = 0;
    int b = 0;
    if (FUZZ_INPUT[0] == 100)
    {

        a = 1;
        b = 2;
        for (int i = 0; i < 10000; i++)
        {
            a += i;
            b += i;
        }
        for (int i = 0; i < 10000; i++)
        {
            a += i;
            b += i;
        }

        volatile int *code_addr = (int *)FUZZ_INPUT[0]; // 获取函数地址（代码段）
        *code_addr = 1; // 修改函数代码（非法访问）
    }



    // --- 后续代码不应被执行到 ---
    // 如果你看到了这条打印，说明错误没有被触发
    printf("FuzzTask: ERROR - The fault was not triggered. Reached BREAKPOINT.\n");
    // 在这里实现你的模糊测试逻辑:
    // 使用 FUZZ_INPUT 中的数据来调用 FreeRTOS API 或其他目标函数
    //
    // 例如:
    // if (MAX_FUZZ_INPUT_SIZE > 0 && FUZZ_INPUT[0] < 10) {
    //     TickType_t xTicksToDelay = (FUZZ_INPUT[0] + 1) * 10;
    //     printf("Harness: Delaying for %u ticks.\n", (unsigned int)xTicksToDelay);
    //     fflush(stdout);
    //     vTaskDelay(xTicksToDelay);
    // }
    //
    // if (MAX_FUZZ_INPUT_SIZE > 2 && FUZZ_INPUT[1] == 0xAA) {
    //    TaskHandle_t xSomeTaskHandle = xTaskGetCurrentTaskHandle();
    //    UBaseType_t uxPriority = uxTaskPriorityGet(xSomeTaskHandle);
    //    if (FUZZ_INPUT[2] < configMAX_PRIORITIES) {
    //        vTaskPrioritySet(xSomeTaskHandle, FUZZ_INPUT[2]);
    //        vTaskPrioritySet(xSomeTaskHandle, uxPriority); // Restore
    //    }
    // }
    // 等等...
    // ====================================================================

    // 调用 BREAKPOINT 函数通知 LibAFL 一个 fuzz case 结束
    // printf("a: %d, b: %d\n", a, b);
    // fflush(stdout);
    // printf("Harness: BREAKPOINT() called. LibAFL will reset and re-enter fuzz_task().\n");
    // fflush(stdout);
    printf("into BREAKPOINT\n");
    BREAKPOINT();
}




/* ------------------------------------------------------------------------- */
/* --- FreeRTOS Task to host the fuzzing harness --- */
/* ------------------------------------------------------------------------- */

// 这个任务函数是为了给 fuzz_task() 提供一个 FreeRTOS 的运行环境。
// LibAFL QEMU 会直接把 PC 指向 fuzz_task() 来开始每个测试。
// 这个函数会被 app_main.c 调用，用来创建和启动我们的 FuzzHostTask。
// void vInitialiseFuzzHarness(void) {

//     printf("Harness: Initializing fuzz harness...\n");
//     fflush(stdout);

//     BaseType_t xReturned = xTaskCreate(
//         prvFuzzHostTask,                /* 任务函数. */
//         "FuzzHost",                     /* 任务名. */
//         configMINIMAL_STACK_SIZE * 4,   /* 栈大小 (以字为单位). 如果 harness 复杂，可能需要增加. */
//         NULL,                           /* 传递给任务的参数. */
//         tskIDLE_PRIORITY + 2,           /* 任务优先级. 比 Idle 高，如果 MPU demo 任务也运行，确保优先级合理. */
//         NULL);                          /* 用于传出任务句柄 (如果需要). */

//     if (xReturned != pdPASS) {
//         printf("Harness: FATAL - Failed to create FuzzHostTask!\n");
//         fflush(stdout);
//         // 如果任务创建失败是致命的，可以在这里死循环或触发断言/故障。
//         for(;;);
//     } else {
//         printf("Harness: FuzzHostTask created successfully.\n");
//         fflush(stdout);
//     }

// }
// At the top of fuzz_harness.c or in a shared scope
#define FUZZ_HOST_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 4) // Or start smaller, e.g., * 2
static StackType_t xFuzzHostTaskStack[FUZZ_HOST_TASK_STACK_DEPTH];
static StaticTask_t xFuzzHostTaskTCB; // Buffer for TCB

// In vInitialiseFuzzHarness()
void fuzz_task(void) {
    // ... (伪引用和打印) ...
    // printf("Harness: Attempting to create FuzzHostTask STATICALLY. Stack depth: %u words.\n",
    //        (unsigned int)FUZZ_HOST_TASK_STACK_DEPTH);
    fflush(stdout);
    TaskHandle_t xHandle = xTaskCreateStatic(
                                test_task,
                                "FuzzHostTask",
                                FUZZ_HOST_TASK_STACK_DEPTH, // Stack depth in words
                                NULL,                       // Parameters
                                tskIDLE_PRIORITY + 2,       // Priority
                                xFuzzHostTaskStack,         // Pointer to stack array
                                &xFuzzHostTaskTCB);         // Pointer to TCB buffer
   

    if (xHandle == NULL) { // xTaskCreateStatic returns NULL on failure
        printf("Harness: FATAL - Failed to create FuzzHostTask STATICALLY!\n");
        fflush(stdout);
        for(;;);
    } else {
        printf("Harness: FuzzHostTask created successfully STATICALLY (Handle: %p).\n", (void*)xHandle);
        fflush(stdout);
    }
    vTaskStartScheduler();
}