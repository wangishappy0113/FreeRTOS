/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-23 18:59:56
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_uxTaskGetStackHighWaterMark_StaticTask_Fuzz.c
//    测试名称: uxTaskGetStackHighWaterMark_StaticTask_Fuzz
//    API类别: task_management
//    描述: Fuzzes uxTaskGetStackHighWaterMark by creating a static worker task with varying priorities and stack consumption depths. The test queries the high water mark of both the created task (via handle) and the fuzzer task itself (via NULL). It ensures the returned watermark is within valid bounds and that the API handles different task states safely.


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
