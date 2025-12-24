/*
 * RTOS模糊测试 - 单个测试用例
 * 文件: vPortEnterCritical_Reentrancy_Fuzz
 * 生成时间: 2025-12-24 15:52:58
 * 目标RTOS: FreeRTOS
 * 项目路径: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC
 * API类别: task_management
 */

/*
 * 测试用例详细信息:
 * 名称: vPortEnterCritical_Reentrancy_Fuzz
 * 描述: Tests the reentrancy and privilege elevation logic of vPortEnterCritical and vPortExitCritical on MPU-enabled systems. It uses fuzz input to vary nesting depth and simulates the environment where vAssertCalled is invoked, ensuring the system handles multiple nested critical sections and subsequent unwinding without stack or privilege corruption.
 * 
 * 生成上下文:
 * 检测到的API函数: vAssertCalled, SCB_CFSR_IACCVIOL_Msk              , __CMSIS_GCC_OUT_REG, CMSDK_SRAM_BASE         , SCB_CFSR_UNALIGNED_Msk            , SysTick_CTRL_ENABLE_Msk            , SCB_SHCSR_MEMFAULTPENDED_Msk       , SCB_CFSR_DACCVIOL_Msk              , CMSDK_DUALTIMER1_CTRL_PRESCALE_Msk   , __LDAB
 * 主要文件: /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/main.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/main_full.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/main.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/main_test.c, /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPS2_QEMU_IAR_GCC/main_blinky.c
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

// 本模板专用于 FreeRTOS CORTEX_MPS2_QEMU_IAR_GCC Demo（非 MPU 端口）
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

// LibAFL集成必需的全局变量和函数
#define MAX_FUZZ_INPUT_SIZE 1024
__attribute__((used, visibility("default"))) unsigned char FUZZ_INPUT[MAX_FUZZ_INPUT_SIZE] = {
    // 默认种子
    0x46, 0x55, 0x5a, 0x5a, 0x01, 0x23, 0x45, 0x67,
    0x89, 0xab, 0xcd, 0xef, 0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc,
    0xdd, 0xee, 0xff, 0x00, 0x13, 0x37, 0x42, 0x24,
    0x5a, 0xa5, 0xc3, 0x3c, 0xde, 0xed, 0xbe, 0xef,
    0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
    0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x0f,
    0x1f, 0x2f, 0x3f, 0x4f, 0x5f, 0x6f, 0x7f, 0x8f
};

// LibAFL断点函数
int __attribute__((noinline, used, visibility("default"))) BREAKPOINT(void)
{
    for (;;) {
        __asm volatile("nop");
    }
}

#define FUZZ_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 4)
static StackType_t xFuzzTaskStack[FUZZ_TASK_STACK_DEPTH];
static StaticTask_t xFuzzTaskTCB;
#define FUZZ_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

// ====================================================================
// 测试用例辅助函数和全局变量（统一前缀 FR_*）
// ====================================================================
typedef struct {
    const unsigned char* data;
    size_t size;
    size_t off;
} FR_Reader;

#define FR_init_SELECT(_1,_2,NAME,...) NAME
#define FR_init(...) FR_init_SELECT(__VA_ARGS__, FR_init_2, FR_init_1)(__VA_ARGS__)
static inline FR_Reader FR_init_1(const unsigned char* buf) { FR_Reader r = { buf, MAX_FUZZ_INPUT_SIZE, 0 }; return r; }
static inline FR_Reader FR_init_2(const unsigned char* buf, size_t n) { FR_Reader r = { buf, n, 0 }; return r; }
static inline size_t FR_remaining(FR_Reader* r) { return (r->off < r->size) ? (r->size - r->off) : 0; }
static inline uint8_t FR_next_u8(FR_Reader* r) {
    if (r->off + 1 <= r->size) return r->data[r->off++];
    return 0;
}
static inline uint16_t FR_next_u16(FR_Reader* r) {
    uint16_t lo = FR_next_u8(r);
    uint16_t hi = FR_next_u8(r);
    return (uint16_t)((hi << 8) | lo);
}
static inline uint32_t FR_next_u32(FR_Reader* r) {
    uint32_t lo = FR_next_u16(r);
    uint32_t hi = FR_next_u16(r);
    return (hi << 16) | lo;
}
static inline uint32_t FR_next_range(FR_Reader* r, uint32_t min_v, uint32_t max_v) {
    if (max_v <= min_v) return min_v;
    uint32_t span = max_v - min_v + 1u;
    return min_v + (FR_next_u32(r) % span);
}
static inline size_t FR_next_bytes(FR_Reader* r, unsigned char* out, size_t n) {
    size_t rem = FR_remaining(r); if (n > rem) n = rem; if (n) { memcpy(out, r->data + r->off, n); r->off += n; } return n;
}

static uint32_t uxAtomicCounter = 0;

static void prvSimulateActivity(uint8_t action) {
    switch(action % 4) {
        case 0: (void)uxTaskGetNumberOfTasks(); break;
        case 1: (void)xTaskGetTickCount(); break;
        case 2: uxAtomicCounter++; break;
        case 3: break;
    }
}

// ====================================================================
// 主测试函数（固定骨架 + 有界迭代）
// ====================================================================
void __attribute__((used, visibility("default"))) test_task(void *pvParameters)
{
    (void)pvParameters;
    
    // 统一使用 FUZZ_INPUT 构造 Reader
    FR_Reader fr_local = FR_init(FUZZ_INPUT, MAX_FUZZ_INPUT_SIZE);

    unsigned char fr_baseline[16] = {0};
    (void)FR_next_bytes(&fr_local, fr_baseline, sizeof(fr_baseline));

    // 统一的有界迭代次数
    unsigned int iterations = (unsigned int)FR_next_range(&fr_local, 1, 10);

    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        return;
    }

    for (unsigned int i = 0; i < iterations; ++i) {
        if (FR_remaining(&fr_local) < 5) {
            break;
        }

        /* Nesting depth between 1 and 16 to test uxCriticalNesting limits */
        uint8_t uxNestDepth = (uint8_t)FR_next_range(&fr_local, 1, 16);
        uint8_t ucActivityMask = FR_next_u8(&fr_local);
        uint8_t ucAssertTrigger = FR_next_u8(&fr_local);

        /* Enter nested critical sections */
        for (uint8_t j = 0; j < uxNestDepth; j++) {
            vPortEnterCritical();
    
            if (ucActivityMask & (1 << (j % 8))) {
                prvSimulateActivity(j);
            }
        }

        configASSERT(uxNestDepth > 0);

        /* Unwind the critical sections */
        for (uint8_t j = 0; j < uxNestDepth; j++) {
            vPortExitCritical();
        }

        /* Test vAssertCalled logic */
        if (ucAssertTrigger == 0xAD) {
            vPortEnterCritical();
            vAssertCalled(__FILE__, __LINE__);
            vPortExitCritical();
        }

        if (FR_remaining(&fr_local) > 0) {
            (void)uxTaskPriorityGet(NULL);
        }
    }

    printf("[TEST_CASE_COMPLETED]\n");
    fflush(stdout);
    BREAKPOINT();
}

// FreeRTOS任务包装器
void fuzz_task(void)
{
    TaskHandle_t xHandle = xTaskCreateStatic(
        test_task,
        "FuzzTask",
        FUZZ_TASK_STACK_DEPTH,
        NULL,
        FUZZ_TASK_PRIORITY,
        xFuzzTaskStack,
        &xFuzzTaskTCB
    );

    if (xHandle == NULL) {
        printf("FuzzTask creation failed\n"); 
        fflush(stdout);
        configASSERT(0);
        for (;;) {
        }
    }

    vTaskStartScheduler();

    for (;;) {
    }
}