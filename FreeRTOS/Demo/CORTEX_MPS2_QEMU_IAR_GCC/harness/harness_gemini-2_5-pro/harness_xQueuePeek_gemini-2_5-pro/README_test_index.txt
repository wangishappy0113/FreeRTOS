/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-19 17:59:31
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_xQueuePeek_FuzzTest.c
//    测试名称: xQueuePeek_FuzzTest
//    API类别: queue
//    描述: Fuzzes xQueuePeek by testing peeking on empty, non-empty, and blocking scenarios. A helper task sends data to the queue while the main task is blocked, testing timeout and success conditions. Verifies that data is not removed from the queue after a successful peek and that blocking durations are respected.


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
