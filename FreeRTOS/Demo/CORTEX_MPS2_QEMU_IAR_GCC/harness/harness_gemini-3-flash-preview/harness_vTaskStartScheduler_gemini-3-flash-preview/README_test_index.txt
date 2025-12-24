/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-23 19:42:16
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_vTaskStartScheduler_StaticInit_Fuzz.c
//    测试名称: vTaskStartScheduler_StaticInit_Fuzz
//    API类别: task_management
//    描述: Fuzzes the scheduler initialization sequence by manipulating the static memory provider hooks for idle and timer tasks. It forces 'allocation' failures to exercise the return-on-failure logic in vTaskStartScheduler and tests the kernel's robustness when task creation parameters are varied during a simulated re-initialization. This also exercises the logic in prvCreateIdleTasks and xTimerCreateTimerTask.


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
