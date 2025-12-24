/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-18 21:32:58
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_xTaskGetIdleTaskHandle_basic_idle_handle_validation.c
//    测试名称: xTaskGetIdleTaskHandle_basic_idle_handle_validation
//    API类别: task_management
//    描述: Fuzzes interactions around xTaskGetIdleTaskHandle by creating a static worker task that optionally yields, delays, and queries the idle task handle multiple times per iteration. Validates that the returned handle is non-NULL, stable across calls, and distinct from the worker task handle. Uses fuzz input to vary delay durations, yield behavior, and the number of handle queries.


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
