#include <stdio.h>
#include <string.h>

#ifdef TARGET_CUSTOM_INSN
#include "lqemu.h"
#endif

#ifndef TARGET_CUSTOM_INSN
int __attribute__((noinline)) BREAKPOINT() {
    for (;;) {}
}
#endif
void divide_by_zero_vulnerability(int divisor) {
    printf("Attempting division: 100 / %d\n", divisor);
    
    // 直接进行除法运算，如果除数为0就会触发错误
    int result = 100 / divisor;
    
    printf("Division result: %d\n", result);
}
// 栈溢出函数
void trigger_stack_overflow() {
    char small_buffer[8]; // 非常小的缓冲区
    
    // 直接写入大量数据，肯定会溢出
    memset(small_buffer, 'A', 100); // 写入100个'A'到8字节的缓冲区
    
    // 再写入一次确保溢出
    strcpy(small_buffer, "This is a very long string that will definitely cause stack overflow");
}

// 安全的函数
void safe_operation() {
    char buffer[8];
    // 安全操作，不会溢出
    strncpy(buffer, "Safe", sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    printf("Safe operation completed: %s\n", buffer);
}

int LLVMFuzzerTestOneInput(unsigned int *Data, unsigned int Size) {
#ifdef TARGET_CUSTOM_INSN
    libafl_qemu_start_phys((void *)Data, Size);
#endif

    /* 根据输入数据判断是否触发栈溢出 */
    if (Size > 1) {
        // 检查第一个元素的奇偶性来决定是否触发栈溢出
            if (Data[0] % 2 == 0) {
                // 偶数：触发栈溢出
                if(Data[1]%2==0){
                    printf("Even input1 (%u)and even input2(%c): Triggering divide zero\n", Data[0],Data[1]);
                    divide_by_zero_vulnerability(0);
                    }
                else{
                    printf("Even input (%u)and odd input2(%c): Triggering stack overflow\n", Data[0],Data[1]);
                    trigger_stack_overflow();
                }
            } else {
                // 奇数：执行安全操作
                printf("Odd input (%u): Performing safe operation\n", Data[0]);
                safe_operation();
            }
    } else {
        // 空输入也执行安全操作
        printf("Empty input: Performing safe operation\n");
        safe_operation();
    }

    /* 超时条件测试 */

    /* 执行 Hello World */
    printf("Hello World!\n");

#ifdef TARGET_CUSTOM_INSN
    libafl_qemu_end(LIBAFL_QEMU_END_OK);
#else
    return BREAKPOINT();
#endif
}

unsigned int FUZZ_INPUT[] = {
     175, 175, 175, 19, 255, 255, 255, 127, 18, 0, 0, 255, 255, 1
};

int main(void) {
    LLVMFuzzerTestOneInput(FUZZ_INPUT, 50);
    return 0;
}