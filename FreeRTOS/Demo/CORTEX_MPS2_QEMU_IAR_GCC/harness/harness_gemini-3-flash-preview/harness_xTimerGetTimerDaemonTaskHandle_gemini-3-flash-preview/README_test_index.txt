/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-23 20:50:56
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_xTimerGetTimerDaemonTaskHandle_Lifecycle_Fuzz.c
//    测试名称: xTimerGetTimerDaemonTaskHandle_Lifecycle_Fuzz
//    API类别: timers
//    描述: Retrieves the software timer daemon task handle and performs fuzzed priority adjustments and static timer lifecycle operations. This test case exercises the daemon task's responsiveness to scheduling changes and its ability to handle timer command queue operations (start, stop, delete) under fuzzed priorities and periods.


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
