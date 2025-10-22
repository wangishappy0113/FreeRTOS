#!/usr/bin/env python3
"""
FreeRTOS API 知识库构建器 - 第一阶段
从 Joern CPG 中提取完整的 API 信息
"""

import subprocess
import json
import re
from pathlib import Path
from datetime import datetime

class FreeRTOSAPIExtractor:
    def __init__(self, cpg_path='with_compdb_cpg.bin'):
        self.cpg_path = cpg_path
        self.base_dir = '/home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPU_M3_MPS2_QEMU_GCC'
        self.api_database = {
            'metadata': {
                'extraction_date': datetime.now().isoformat(),
                'cpg_source': cpg_path,
                'freertos_version': 'Latest',
                'total_apis': 0
            },
            'modules': {},
            'raw_data': {}
        }

    def run_joern_query(self, query, timeout=30):
        """运行 Joern 查询"""
        cmd = f'echo \'{query}\' | joern {self.cpg_path}'
        try:
            result = subprocess.run(
                cmd, shell=True, capture_output=True, text=True, 
                cwd=self.base_dir, timeout=timeout
            )
            return result.stdout
        except Exception as e:
            print(f"查询失败: {e}")
            return ""

    def extract_public_apis(self):
        """提取所有公共 API 函数"""
        print("🔍 提取 FreeRTOS 公共 API...")
        
        # 查询所有以 x、v、u、e 开头的函数 (FreeRTOS API 命名规范)
        api_prefixes = ['x', 'v', 'u', 'e', 'pv']
        
        all_apis = {}
        
        for prefix in api_prefixes:
            query = f'cpg.method.name("{prefix}.*").name.l'
            output = self.run_joern_query(query)
            
            # 解析输出
            methods = re.findall(r'"([^"]*)"', output)
            for method in methods:
                if method.startswith(prefix) and not method.startswith('MPU_'):
                    # 分类 API
                    category = self.categorize_api(method)
                    if category not in all_apis:
                        all_apis[category] = []
                    all_apis[category].append(method)
        
        self.api_database['raw_data']['public_apis'] = all_apis
        return all_apis

    def categorize_api(self, api_name):
        """根据 API 名称进行分类"""
        if 'Task' in api_name or api_name.startswith('xTask') or api_name.startswith('vTask') or api_name.startswith('eTask'):
            return 'Tasks'
        elif 'Queue' in api_name or api_name.startswith('xQueue') or api_name.startswith('vQueue'):
            return 'Queues'
        elif 'Semaphore' in api_name or api_name.startswith('xSemaphore') or api_name.startswith('vSemaphore'):
            return 'Semaphores'
        elif 'Timer' in api_name or api_name.startswith('xTimer') or api_name.startswith('vTimer'):
            return 'Timers'
        elif 'EventGroup' in api_name or 'Event' in api_name:
            return 'Event Groups'
        elif 'StreamBuffer' in api_name or 'Stream' in api_name:
            return 'Stream Buffers'
        elif 'Port' in api_name or api_name.startswith('pv') or 'Malloc' in api_name or 'Free' in api_name:
            return 'Memory Management'
        elif 'Notify' in api_name:
            return 'Notifications'
        elif 'Mutex' in api_name:
            return 'Mutexes'
        else:
            return 'Other'

    def extract_api_signatures(self, apis):
        """提取 API 函数签名"""
        print("📝 提取 API 函数签名...")
        
        signatures = {}
        
        for category, api_list in apis.items():
            print(f"  处理 {category} 模块...")
            signatures[category] = {}
            
            for api in api_list[:5]:  # 限制每个类别处理前5个，避免太慢
                query = f'cpg.method.name("{api}").signature.l'
                output = self.run_joern_query(query)
                
                # 解析签名
                sig_matches = re.findall(r'"([^"]*)"', output)
                if sig_matches:
                    signatures[category][api] = {
                        'signature': sig_matches[0],
                        'category': category
                    }
                    print(f"    ✅ {api}: {sig_matches[0]}")
                else:
                    print(f"    ❌ {api}: 签名提取失败")
        
        self.api_database['raw_data']['signatures'] = signatures
        return signatures

    def extract_api_parameters(self, apis):
        """提取 API 参数信息"""
        print("🔧 提取 API 参数信息...")
        
        parameters = {}
        
        for category, api_list in apis.items():
            print(f"  处理 {category} 模块参数...")
            parameters[category] = {}
            
            for api in api_list[:3]:  # 每个类别选择前3个进行详细分析
                query = f'cpg.method.name("{api}").parameter.name.l'
                output = self.run_joern_query(query)
                
                param_names = re.findall(r'"([^"]*)"', output)
                
                # 获取参数类型
                query_types = f'cpg.method.name("{api}").parameter.typeFullName.l'
                output_types = self.run_joern_query(query_types)
                param_types = re.findall(r'"([^"]*)"', output_types)
                
                if param_names:
                    param_info = []
                    for i, name in enumerate(param_names):
                        param_type = param_types[i] if i < len(param_types) else "unknown"
                        param_info.append({
                            'name': name,
                            'type': param_type
                        })
                    
                    parameters[category][api] = {
                        'parameters': param_info,
                        'param_count': len(param_names)
                    }
                    print(f"    ✅ {api}: {len(param_names)} 个参数")
        
        self.api_database['raw_data']['parameters'] = parameters
        return parameters

    def extract_call_relationships(self, apis):
        """提取 API 调用关系"""
        print("🔗 分析 API 调用关系...")
        
        relationships = {}
        
        for category, api_list in apis.items():
            print(f"  分析 {category} 调用关系...")
            relationships[category] = {}
            
            for api in api_list[:2]:  # 每个类别选择前2个进行调用关系分析
                # 查询谁调用了这个 API
                query = f'cpg.call.name("{api}").caller.name.l'
                output = self.run_joern_query(query)
                callers = re.findall(r'"([^"]*)"', output)
                
                # 查询这个 API 调用了谁
                query_callees = f'cpg.method.name("{api}").call.name.l'
                output_callees = self.run_joern_query(query_callees)
                callees = re.findall(r'"([^"]*)"', output_callees)
                
                relationships[category][api] = {
                    'called_by': callers[:10],  # 限制前10个调用者
                    'calls': callees[:10],      # 限制前10个被调用者
                    'usage_frequency': len(callers)
                }
                
                print(f"    ✅ {api}: {len(callers)} 个调用者, {len(callees)} 个被调用")
        
        self.api_database['raw_data']['relationships'] = relationships
        return relationships

    def generate_api_summary(self):
        """生成 API 摘要统计"""
        print("📊 生成 API 摘要...")
        
        raw_data = self.api_database['raw_data']
        
        summary = {
            'total_categories': len(raw_data.get('public_apis', {})),
            'apis_by_category': {},
            'most_complex_apis': [],
            'most_used_apis': []
        }
        
        # 统计每个类别的 API 数量
        for category, apis in raw_data.get('public_apis', {}).items():
            summary['apis_by_category'][category] = len(apis)
        
        # 找出最复杂的 API（参数最多）
        param_data = raw_data.get('parameters', {})
        complexity_list = []
        for category, apis in param_data.items():
            for api, info in apis.items():
                complexity_list.append({
                    'api': api,
                    'category': category,
                    'param_count': info.get('param_count', 0)
                })
        
        summary['most_complex_apis'] = sorted(
            complexity_list, 
            key=lambda x: x['param_count'], 
            reverse=True
        )[:10]
        
        # 找出使用最频繁的 API
        rel_data = raw_data.get('relationships', {})
        usage_list = []
        for category, apis in rel_data.items():
            for api, info in apis.items():
                usage_list.append({
                    'api': api,
                    'category': category,
                    'usage_frequency': info.get('usage_frequency', 0)
                })
        
        summary['most_used_apis'] = sorted(
            usage_list,
            key=lambda x: x['usage_frequency'],
            reverse=True
        )[:10]
        
        self.api_database['metadata']['total_apis'] = sum(summary['apis_by_category'].values())
        self.api_database['summary'] = summary
        
        return summary

    def save_knowledge_base(self, filename='freertos_api_knowledge_base.json'):
        """保存知识库到文件"""
        output_path = Path(self.base_dir) / filename
        
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(self.api_database, f, indent=2, ensure_ascii=False)
        
        print(f"💾 知识库已保存到: {output_path}")
        return output_path

    def print_summary(self):
        """打印提取摘要"""
        summary = self.api_database.get('summary', {})
        metadata = self.api_database.get('metadata', {})
        
        print("\n" + "="*60)
        print("🎯 FreeRTOS API 知识库提取完成")
        print("="*60)
        
        print(f"📊 总体统计:")
        print(f"• 提取时间: {metadata.get('extraction_date', 'Unknown')}")
        print(f"• 总 API 数量: {metadata.get('total_apis', 0)}")
        print(f"• API 分类数: {summary.get('total_categories', 0)}")
        
        print(f"\n📋 各模块 API 数量:")
        for category, count in summary.get('apis_by_category', {}).items():
            print(f"• {category}: {count} 个 API")
        
        print(f"\n🏆 最复杂的 API (参数数量):")
        for api_info in summary.get('most_complex_apis', [])[:5]:
            print(f"• {api_info['api']} ({api_info['category']}): {api_info['param_count']} 个参数")
        
        print(f"\n🔥 使用最频繁的 API:")
        for api_info in summary.get('most_used_apis', [])[:5]:
            print(f"• {api_info['api']} ({api_info['category']}): {api_info['usage_frequency']} 次调用")

    def run_extraction(self):
        """运行完整的 API 提取流程"""
        print("🚀 开始 FreeRTOS API 知识库构建...")
        
        try:
            # 第一步：提取公共 API
            apis = self.extract_public_apis()
            
            # 第二步：提取函数签名
            signatures = self.extract_api_signatures(apis)
            
            # 第三步：提取参数信息
            parameters = self.extract_api_parameters(apis)
            
            # 第四步：分析调用关系
            relationships = self.extract_call_relationships(apis)
            
            # 第五步：生成摘要
            summary = self.generate_api_summary()
            
            # 第六步：保存知识库
            output_file = self.save_knowledge_base()
            
            # 第七步：打印摘要
            self.print_summary()
            
            print(f"\n🎉 API 知识库构建完成!")
            print(f"📁 知识库文件: {output_file}")
            print(f"🔧 下一步: 结合官方文档完善参数约束信息")
            
            return True
            
        except Exception as e:
            print(f"❌ 提取过程出错: {e}")
            import traceback
            traceback.print_exc()
            return False

if __name__ == "__main__":
    extractor = FreeRTOSAPIExtractor()
    success = extractor.run_extraction()
    
    if success:
        print("\n✅ 第一阶段完成，可以开始第二阶段：文档集成")
    else:
        print("\n❌ 提取失败，请检查错误信息")
