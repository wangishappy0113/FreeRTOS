/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-24 13:59:32
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_uxQueueMessagesWaitingFromISR_StaticFuzz.c
//    测试名称: uxQueueMessagesWaitingFromISR_StaticFuzz
//    API类别: queue
//    描述: Exercises uxQueueMessagesWaitingFromISR by performing a sequence of ISR-safe queue operations (send and receive) on a statically allocated queue. The test validates that the message count returned by the ISR-specific query API correctly tracks the number of items currently in the queue across multiple state transitions, using parameters derived from fuzz input.


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
