/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-23 15:23:35
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_TaskPriorityGet_InCriticalSection.c
//    测试名称: TaskPriorityGet_InCriticalSection
//    API类别: task_management
//    描述: Fuzzes uxTaskPriorityGet by querying the priority of a statically created worker task and the current task. The test randomly decides whether to perform the query from within a critical section, initiated by vPortEnterCritical. Fuzz inputs control the worker task's priority, privilege level, the choice of task handle to query (worker, self via NULL, or self via handle), and whether a critical section is used.


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
