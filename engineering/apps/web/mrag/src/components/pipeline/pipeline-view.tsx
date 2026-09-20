'use client';

import { Card, CardHeader, CardTitle, CardDescription, CardContent } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Badge } from '@/components/ui/badge';
import { useUIStore } from '@/stores/ui-store';
import { Check } from 'lucide-react';
import { cn } from '@/lib/utils';

const PIPELINES = [
  {
    type: 'naive',
    name: 'Naive RAG',
    description: '基础 RAG：查询 → 向量检索 → 上下文 → LLM',
    complexity: 'low',
  },
  {
    type: 'advanced',
    name: 'Advanced RAG',
    description: '高级 RAG：查询扩展 + 混合检索 + 重排序',
    complexity: 'medium',
  },
  {
    type: 'hybrid',
    name: 'Hybrid RAG',
    description: '三路召回：向量 + BM25 + 知识图谱',
    complexity: 'medium',
  },
  {
    type: 'hyde',
    name: 'HyDE RAG',
    description: '假设答案引导检索',
    complexity: 'medium',
  },
  {
    type: 'graph',
    name: 'Graph RAG',
    description: '知识图谱检索 + 实体提取',
    complexity: 'high',
  },
  {
    type: 'corrective',
    name: 'Corrective RAG',
    description: '检索质量评估 + 自动纠正',
    complexity: 'high',
  },
  {
    type: 'react',
    name: 'ReAct RAG',
    description: '推理 + 行动循环',
    complexity: 'high',
  },
  {
    type: 'iterative',
    name: 'Iterative RAG',
    description: '迭代查询优化',
    complexity: 'high',
  },
  {
    type: 'recursive',
    name: 'Recursive RAG',
    description: '递归分解子问题',
    complexity: 'high',
  },
];

export function PipelineView() {
  const { selectedPipeline, setSelectedPipeline } = useUIStore();

  return (
    <div className="p-6 space-y-6 overflow-y-auto h-full">
      <div>
        <h2 className="text-2xl font-bold">Pipeline 配置</h2>
        <p className="text-sm text-muted-foreground">
          选择 RAG Pipeline 类型，配置检索、重排序、生成参数
        </p>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
        {PIPELINES.map((pipeline) => {
          const isSelected = selectedPipeline === pipeline.type;
          const complexityColor =
            pipeline.complexity === 'low'
              ? 'bg-green-100 text-green-700'
              : pipeline.complexity === 'medium'
              ? 'bg-yellow-100 text-yellow-700'
              : 'bg-red-100 text-red-700';

          return (
            <Card
              key={pipeline.type}
              className={cn(
                'cursor-pointer transition-all hover:shadow-md',
                isSelected && 'border-primary ring-2 ring-primary/20'
              )}
              onClick={() => setSelectedPipeline(pipeline.type)}
            >
              <CardHeader>
                <div className="flex items-center justify-between">
                  <CardTitle className="text-base">{pipeline.name}</CardTitle>
                  {isSelected && (
                    <div className="h-5 w-5 rounded-full bg-primary flex items-center justify-center">
                      <Check className="h-3 w-3 text-primary-foreground" />
                    </div>
                  )}
                </div>
                <CardDescription>{pipeline.description}</CardDescription>
              </CardHeader>
              <CardContent>
                <Badge variant="secondary" className={complexityColor}>
                  {pipeline.complexity === 'low' && '低复杂度'}
                  {pipeline.complexity === 'medium' && '中复杂度'}
                  {pipeline.complexity === 'high' && '高复杂度'}
                </Badge>
              </CardContent>
            </Card>
          );
        })}
      </div>

      {/* 配置面板 */}
      <Card>
        <CardHeader>
          <CardTitle>参数配置</CardTitle>
          <CardDescription>
            当前选中: <Badge variant="default">{selectedPipeline}</Badge>
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-4">
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div>
              <label className="text-sm font-medium">Top K</label>
              <Input type="number" defaultValue="5" min="1" max="50" />
              <p className="text-xs text-muted-foreground mt-1">
                检索返回的文档数量
              </p>
            </div>
            <div>
              <label className="text-sm font-medium">Temperature</label>
              <Input
                type="number"
                defaultValue="0.7"
                min="0"
                max="2"
                step="0.1"
              />
              <p className="text-xs text-muted-foreground mt-1">
                LLM 生成温度（0-2）
              </p>
            </div>
            <div>
              <label className="text-sm font-medium">Max Tokens</label>
              <Input type="number" defaultValue="2048" min="64" max="8192" />
              <p className="text-xs text-muted-foreground mt-1">
                最大生成 token 数
              </p>
            </div>
            <div>
              <label className="text-sm font-medium">RRF K</label>
              <Input type="number" defaultValue="60" min="1" max="200" />
              <p className="text-xs text-muted-foreground mt-1">
                Reciprocal Rank Fusion 参数
              </p>
            </div>
          </div>

          <div className="flex gap-2 pt-4">
            <Button>保存配置</Button>
            <Button variant="outline">恢复默认</Button>
            <Button variant="ghost">测试查询</Button>
          </div>
        </CardContent>
      </Card>
    </div>
  );
}