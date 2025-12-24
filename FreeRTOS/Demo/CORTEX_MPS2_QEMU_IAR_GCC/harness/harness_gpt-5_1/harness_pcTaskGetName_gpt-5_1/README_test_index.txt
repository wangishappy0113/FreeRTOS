/*
 * RTOS模糊测试 - 测试用例索引
 * 生成时间: 2025-12-18 22:33:41
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * 
 * 本目录包含 1 个独立的测试用例文件
 */

// 测试用例列表:

// 1. test_00_pcTaskGetName_basic_and_null_handle_fuzz.c
//    测试名称: pcTaskGetName_basic_and_null_handle_fuzz
//    API类别: task_management
//    描述: Fuzz pcTaskGetName by querying names of multiple statically created tasks and the calling task (NULL handle), using fuzz input to select which handle to query and to vary task names and priorities. Verifies returned pointers and string properties without violating FreeRTOS constraints.


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
