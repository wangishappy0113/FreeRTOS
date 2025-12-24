/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-24 15:46:12
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_xStreamBufferIsEmpty_ConsistencyCheck.c
//    测试名称: xStreamBufferIsEmpty_ConsistencyCheck
//    API类别: queue
//    描述: Fuzzes xStreamBufferIsEmpty by performing a sequence of Send, Receive, and Reset operations on a statically allocated stream buffer. The test validates that xStreamBufferIsEmpty consistently returns pdTRUE if and only if xStreamBufferBytesAvailable returns zero, exploring various states of buffer fullness and wrapping.


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
