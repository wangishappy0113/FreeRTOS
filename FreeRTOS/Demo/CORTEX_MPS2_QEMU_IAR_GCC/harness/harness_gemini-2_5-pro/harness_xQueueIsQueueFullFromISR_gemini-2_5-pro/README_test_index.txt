/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-19 17:53:15
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_fuzz_xQueueIsQueueFullFromISR.c
//    测试名称: fuzz_xQueueIsQueueFullFromISR
//    API类别: queue
//    描述: Fuzzes xQueueIsQueueFullFromISR by creating a static queue with fuzzed length and item size. The test fills the queue with a fuzzed number of items and then calls the target API from a simulated ISR context (by raising BASEPRI). It asserts that the function correctly reports whether the queue is full or not, both when partially and completely full.


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
