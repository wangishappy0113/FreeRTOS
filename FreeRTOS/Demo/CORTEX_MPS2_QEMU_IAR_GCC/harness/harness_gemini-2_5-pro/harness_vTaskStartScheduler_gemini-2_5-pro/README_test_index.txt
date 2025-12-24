/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-23 16:23:26
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_Fuzz_vTaskStartScheduler_Preconditions.c
//    测试名称: Fuzz_vTaskStartScheduler_Preconditions
//    API类别: scheduler_management
//    描述: Fuzzes the preconditions for vTaskStartScheduler. This test defines the static memory allocation hooks (vApplicationGetIdleTaskMemory, vApplicationGetTimerTaskMemory) required when using static allocation. In each iteration, it uses fuzz input to maliciously configure the parameters returned by these hooks (e.g., return NULL pointers or invalid stack sizes). It also creates a variable number of application tasks with fuzzed priorities. The goal is to trigger assertions or undefined behavior within vTaskStartScheduler, particularly in the prvCreateIdleTasks and xTimerCreateTimerTask functions, by creating invalid initial system states before the scheduler is started.


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
