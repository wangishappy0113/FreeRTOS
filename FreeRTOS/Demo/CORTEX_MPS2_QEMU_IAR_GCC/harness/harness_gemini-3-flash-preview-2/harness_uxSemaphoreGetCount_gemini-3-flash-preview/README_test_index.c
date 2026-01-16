/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-26 01:19:08
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_uxSemaphoreGetCount_StaticFuzz.c
//    测试名称: uxSemaphoreGetCount_StaticFuzz
//    API类别: semaphore
//    描述: Fuzzes uxSemaphoreGetCount by performing a sequence of Give and Take operations on both binary and counting semaphores. It validates that the count returned by uxSemaphoreGetCount never exceeds the maximum count specified during creation and handles both binary and counting semaphore types using static allocation.


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
