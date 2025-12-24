/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-23 14:01:27
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_fuzz_task_and_queue_isr_operations.c
//    测试名称: fuzz_task_and_queue_isr_operations
//    API类别: task_management
//    描述: Fuzzes uxTaskGetNumberOfTasks by creating/deleting tasks and verifies its count throughout the lifecycle. Also fuzzes xQueueGenericSendFromISR and xQueueGiveFromISR by creating static tasks that block on a queue and a binary semaphore. The test logic, running in a privileged task, simulates an ISR context to send/give to these objects, waking the worker tasks. Fuzzer input determines the number of worker tasks, queue length, and queue send position (front, back, or overwrite). Asserts check task counts, successful ISR operations, and that worker tasks were correctly unblocked.


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
