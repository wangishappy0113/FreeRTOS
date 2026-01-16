/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-26 08:09:01
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_vStreamBufferDelete_Static_Lifecycle.c
//    测试名称: vStreamBufferDelete_Static_Lifecycle
//    API类别: stream_buffer
//    描述: Fuzzes the lifecycle of a statically allocated stream buffer, focusing on vStreamBufferDelete. It varies the trigger level and initial data before deletion to ensure the cleanup logic (memsetting the control structure) handles various states correctly in a Cortex-M MPU environment.


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
