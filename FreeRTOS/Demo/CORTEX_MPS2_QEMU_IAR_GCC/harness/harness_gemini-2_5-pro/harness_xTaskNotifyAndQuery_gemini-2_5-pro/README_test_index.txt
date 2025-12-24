/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-23 15:19:52
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_fuzz_uxTaskGetSystemState_and_vTaskSuspendAll.c
//    测试名称: fuzz_uxTaskGetSystemState_and_vTaskSuspendAll
//    API类别: task_management
//    描述: Fuzzes uxTaskGetSystemState by creating multiple tasks and putting them into various states (Ready, Blocked, Suspended). It calls uxTaskGetSystemState with fuzzed parameters for the array size and runtime pointer. It also fuzzes the nesting level of vTaskSuspendAll/xTaskResumeAll calls that wrap the main API call. The test validates the number of populated entries and performs basic sanity checks on the returned task status information.


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
