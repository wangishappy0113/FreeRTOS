/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-24 13:47:04
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_fuzz_task_system_state_and_suspension.c
//    测试名称: fuzz_task_system_state_and_suspension
//    API类别: task_management
//    描述: Fuzzes uxTaskGetSystemState and vTaskSuspendAll by exercising scheduler suspension nesting, varying task status array sizes, and toggling runtime stats collection. Uses static task creation and validates the consistency of reported task counts.


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
