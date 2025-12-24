/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-24 14:15:54
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_EventGroup_Heap_Allocation_Fuzz.c
//    测试名称: EventGroup_Heap_Allocation_Fuzz
//    API类别: event_groups
//    描述: Fuzzes xEventGroupCreate and pvPortMalloc by performing multiple dynamic allocations and event group operations based on fuzzed sizes and bitmask parameters. This test ensures that the kernel properly handles dynamic object creation and memory allocation under varying size requests and bit configurations.


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
