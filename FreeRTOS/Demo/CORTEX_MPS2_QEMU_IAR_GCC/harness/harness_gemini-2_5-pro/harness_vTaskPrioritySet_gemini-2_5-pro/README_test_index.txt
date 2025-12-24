/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-23 15:26:56
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_fuzz_vTaskPrioritySet_comprehensive.c
//    测试名称: fuzz_vTaskPrioritySet_comprehensive
//    API类别: task_management
//    描述: Fuzzes vTaskPrioritySet by creating a helper 'worker' task and repeatedly changing either its own priority or the worker's priority. The fuzz input determines the target task (self or worker), the new priority (including out-of-range values to test capping), and the state of the worker task before the change (Ready/Blocked, Suspended, or Deleted). It verifies the priority is set correctly using uxTaskPriorityGet. The test for a deleted task handle is expected to trigger a configASSERT.


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
