/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-23 15:46:01
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_fuzz_xTaskResumeAll_nesting_and_pending.c
//    测试名称: fuzz_xTaskResumeAll_nesting_and_pending
//    API类别: task_management
//    描述: Fuzzes xTaskResumeAll by testing nested scheduler suspension, handling of pending tasks, and processing of pended ticks. A worker task is created with a fuzzed priority (higher, same, or lower than the test task). The test logic nests calls to vTaskSuspendAll, then unblocks the worker task via a semaphore, and simulates pended ticks with vTaskDelay. It then unwinds the nested calls to xTaskResumeAll, asserting that intermediate calls return pdFALSE. The final call's return value is validated to correctly reflect whether a context switch to the higher-priority worker was pending. Task execution flags are checked post-resume to confirm correct scheduling behavior.


/*
 * 编译说明:
 * 1. 每个测试文件都是独立的，可以单独编译
 * 2. 需要链接对应的RTOS库和项目代码
 * 3. 使用LibAFL或其他模糊测试工具进行测试
 * 
 * 使用方法:
 * 1. 选择要测试的API类别
 * 2. 编译对应的测试文件
 * 3. 在模糊测试环境中运行
 */
