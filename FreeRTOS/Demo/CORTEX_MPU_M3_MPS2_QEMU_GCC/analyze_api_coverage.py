#!/usr/bin/env python3
"""
FreeRTOS API 知识库完整性分析脚本
分析当前 Joern 分析的 API 覆盖情况，确定是否满足模糊测试需求
"""

import subprocess
import json
import re
from pathlib import Path

def run_joern_query(cpg_path, query):
    """运行 Joern 查询"""
    cmd = f'echo -e \'{query}\\nexit\' | joern {cpg_path}'
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd='/home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPU_M3_MPS2_QEMU_GCC')
        return result.stdout
    except Exception as e:
        print(f"查询失败: {e}")
        return ""

def analyze_freertos_api_coverage():
    """分析 FreeRTOS API 覆盖情况"""
    
    print("🔍 FreeRTOS API 知识库完整性分析")
    print("=" * 80)
    
    # 定义 FreeRTOS 核心 API 模块
    api_modules = {
        'Tasks': ['xTaskCreate', 'vTaskDelete', 'vTaskDelay', 'vTaskSuspend', 'vTaskResume', 
                  'uxTaskPriorityGet', 'vTaskPrioritySet', 'vTaskStartScheduler'],
        'Queues': ['xQueueCreate', 'xQueueSend', 'xQueueReceive', 'vQueueDelete', 
                   'xQueueSendFromISR', 'xQueueReceiveFromISR'],
        'Semaphores': ['xSemaphoreCreateBinary', 'xSemaphoreCreateMutex', 'xSemaphoreGive', 
                       'xSemaphoreTake', 'vSemaphoreDelete'],
        'Event Groups': ['xEventGroupCreate', 'xEventGroupSetBits', 'xEventGroupWaitBits', 
                         'xEventGroupClearBits', 'vEventGroupDelete'],
        'Timers': ['xTimerCreate', 'xTimerStart', 'xTimerStop', 'xTimerDelete', 
                   'xTimerReset', 'vTimerSetReloadMode'],
        'Stream Buffers': ['xStreamBufferCreate', 'xStreamBufferSend', 'xStreamBufferReceive', 
                           'vStreamBufferDelete'],
        'Memory Management': ['pvPortMalloc', 'vPortFree', 'xPortGetFreeHeapSize'],
        'Notifications': ['xTaskNotifyGive', 'ulTaskNotifyTake', 'xTaskNotify', 'xTaskNotifyWait'],
        'Co-routines': ['xCoRoutineCreate', 'vCoRoutineSchedule', 'crQUEUE_SEND', 'crQUEUE_RECEIVE']
    }
    
    # 当前分析中的核心源文件
    current_source_files = [
        'event_groups.c',
        'tasks.c', 
        'queue.c',
        'timers.c',
        'list.c',
        'stream_buffer.c',
        'heap_4.c'
    ]
    
    # 缺失的核心源文件
    missing_source_files = [
        'croutine.c'  # Co-routines 支持
    ]
    
    # 检查 API 覆盖情况
    cpg_path = 'with_compdb_cpg.bin'
    
    print("📊 当前 API 覆盖情况分析:")
    print("-" * 40)
    
    total_apis = 0
    covered_apis = 0
    
    for module, apis in api_modules.items():
        print(f"\n🔹 {module} 模块:")
        total_apis += len(apis)
        module_covered = 0
        
        for api in apis:
            # 查询是否存在该 API
            query = f'cpg.method.name(".*{api}.*").size'
            result = run_joern_query(cpg_path, query)
            
            # 简单解析结果
            if re.search(r'val res\d+: Int = (\d+)', result):
                count = int(re.search(r'val res\d+: Int = (\d+)', result).group(1))
                if count > 0:
                    print(f"  ✅ {api}: {count} 个实现")
                    module_covered += 1
                    covered_apis += 1
                else:
                    print(f"  ❌ {api}: 未找到")
            else:
                print(f"  ❓ {api}: 查询失败")
        
        coverage_rate = (module_covered / len(apis)) * 100
        print(f"  📈 模块覆盖率: {coverage_rate:.1f}% ({module_covered}/{len(apis)})")
    
    overall_coverage = (covered_apis / total_apis) * 100
    print(f"\n📈 总体 API 覆盖率: {overall_coverage:.1f}% ({covered_apis}/{total_apis})")
    
    return {
        'overall_coverage': overall_coverage,
        'covered_apis': covered_apis,
        'total_apis': total_apis,
        'current_files': current_source_files,
        'missing_files': missing_source_files
    }

def recommend_solution(analysis_result):
    """基于分析结果推荐解决方案"""
    
    print("\n" + "=" * 80)
    print("🎯 模糊测试知识库构建建议")
    print("=" * 80)
    
    coverage = analysis_result['overall_coverage']
    
    if coverage >= 85:
        print("✅ 当前分析已覆盖大部分核心 API，基本满足模糊测试需求")
        print("\n📋 推荐行动方案:")
        print("1. 使用当前的 Joern 分析结果作为基础知识库")
        print("2. 补充缺失的 Co-routines API (如果需要)")
        print("3. 结合官方文档构建完整的 API 参数和返回值信息")
        print("4. 开始构建模糊测试驱动程序")
        
    elif coverage >= 70:
        print("⚠️ 当前分析覆盖了主要 API，但存在一些缺失")
        print("\n📋 推荐行动方案:")
        print("1. 在当前基础上补充缺失的源文件分析")
        print("2. 创建包含完整 FreeRTOS 源码的专门分析项目")
        print("3. 合并两个分析结果构建完整知识库")
        
    else:
        print("❌ 当前分析存在较大 API 覆盖缺失")
        print("\n📋 推荐行动方案:")
        print("1. 必须创建新的全面分析项目")
        print("2. 包含所有 FreeRTOS 核心源文件")
        print("3. 重新构建 compile_commands.json")
    
    print(f"\n📊 具体缺失:")
    print(f"• 缺失源文件: {', '.join(analysis_result['missing_files'])}")
    
    print(f"\n🚀 下一步行动:")
    if coverage >= 80:
        print("• 继续使用当前分析结果")
        print("• 创建 API 文档映射脚本")
        print("• 开始模糊测试驱动生成")
    else:
        print("• 创建全面的 FreeRTOS 源码分析项目")
        print("• 重新配置编译环境")
        print("• 重新运行 Joern 分析")

if __name__ == "__main__":
    result = analyze_freertos_api_coverage()
    recommend_solution(result)
