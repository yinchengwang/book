'use client';

import { useState } from 'react';
import { Card, CardHeader, CardTitle, CardDescription, CardContent } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { Tabs, TabsList, TabsTrigger, TabsContent } from '@/components/ui/tabs';
import { Play, AlertCircle, CheckCircle } from 'lucide-react';

export function EvaluationView() {
  const [activeTab, setActiveTab] = useState('overview');

  return (
    <div className="p-6 space-y-6 overflow-y-auto h-full">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold">评估系统</h2>
          <p className="text-sm text-muted-foreground">
            测试集管理、评估运行、指标对比与优化建议
          </p>
        </div>
        <Button>
          <Play className="h-4 w-4 mr-2" />
          运行评估
        </Button>
      </div>

      <Tabs value={activeTab} onValueChange={setActiveTab}>
        <TabsList>
          <TabsTrigger value="overview" active={activeTab === 'overview'} onClick={() => setActiveTab('overview')}>
            概览
          </TabsTrigger>
          <TabsTrigger value="datasets" active={activeTab === 'datasets'} onClick={() => setActiveTab('datasets')}>
            测试集
          </TabsTrigger>
          <TabsTrigger value="runs" active={activeTab === 'runs'} onClick={() => setActiveTab('runs')}>
            运行历史
          </TabsTrigger>
          <TabsTrigger value="compare" active={activeTab === 'compare'} onClick={() => setActiveTab('compare')}>
            对比分析
          </TabsTrigger>
          <TabsTrigger value="optimize" active={activeTab === 'optimize'} onClick={() => setActiveTab('optimize')}>
            优化建议
          </TabsTrigger>
        </TabsList>

        <TabsContent value="overview" active={activeTab === 'overview'}>
          <EmptyTab
            title="概览指标"
            description="后端评估端点待实现；前端将在数据源可用后接入 Precision、Recall、MRR、Faithfulness 等指标"
          />
        </TabsContent>
        <TabsContent value="datasets" active={activeTab === 'datasets'}>
          <EmptyTab
            title="测试集管理"
            description="后端 Golden Dataset 端点待实现；前端将在数据源可用后展示测试集列表与版本管理"
          />
        </TabsContent>
        <TabsContent value="runs" active={activeTab === 'runs'}>
          <EmptyTab
            title="评估运行历史"
            description="后端评估运行端点待实现；前端将在数据源可用后展示历史评估记录与详细指标"
          />
        </TabsContent>
        <TabsContent value="compare" active={activeTab === 'compare'}>
          <EmptyTab
            title="评估对比"
            description="后端评估对比端点待实现；前端将在数据源可用后展示两次评估的指标对比"
          />
        </TabsContent>
        <TabsContent value="optimize" active={activeTab === 'optimize'}>
          <EmptyTab
            title="优化建议"
            description="后端优化建议端点待实现；前端将在数据源可用后展示基于评估指标生成的改进建议"
          />
        </TabsContent>
      </Tabs>
    </div>
  );
}

function EmptyTab({ title, description }: { title: string; description: string }) {
  return (
    <Card>
      <CardHeader>
        <CardTitle>{title}</CardTitle>
      </CardHeader>
      <CardContent>
        <div className="flex flex-col items-center justify-center text-center py-16 text-muted-foreground">
          <AlertCircle className="h-12 w-12 mb-4 opacity-50" />
          <p className="text-sm">{description}</p>
        </div>
      </CardContent>
    </Card>
  );
}