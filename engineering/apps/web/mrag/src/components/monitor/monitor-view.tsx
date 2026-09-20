'use client';

import { useEffect, useState } from 'react';
import { Card, CardHeader, CardTitle, CardDescription, CardContent } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { Progress } from '@/components/ui/progress';
import { Activity, Database, Cpu, Zap, AlertTriangle, Server, RefreshCw, AlertCircle } from 'lucide-react';
import { apiClient, type SystemStatus } from '@/lib/api';

export function MonitorView() {
  const [status, setStatus] = useState<SystemStatus | null>(null);
  const [metrics, setMetrics] = useState<any>(null);
  const [refreshing, setRefreshing] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const fetchData = async () => {
    setRefreshing(true);
    setError(null);
    try {
      const [statusRes, metricsRes] = await Promise.all([
        apiClient.getStatus(),
        apiClient.getMetrics(),
      ]);
      setStatus(statusRes);
      setMetrics(metricsRes);
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setRefreshing(false);
    }
  };

  useEffect(() => {
    fetchData();
  }, []);

  return (
    <div className="p-6 space-y-6 overflow-y-auto h-full">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold">系统监控</h2>
          <p className="text-sm text-muted-foreground">
            实时查询指标、资源使用、错误率
          </p>
        </div>
        <Button variant="outline" onClick={fetchData} disabled={refreshing}>
          <RefreshCw className={`h-4 w-4 mr-2 ${refreshing ? 'animate-spin' : ''}`} />
          刷新
        </Button>
      </div>

      {error && (
        <Card>
          <CardContent className="pt-6">
            <div className="flex items-center gap-2 text-destructive">
              <AlertCircle className="h-4 w-4" />
              <span className="text-sm">加载失败：{error}</span>
            </div>
          </CardContent>
        </Card>
      )}

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        <Card>
          <CardHeader className="pb-2">
            <CardTitle className="text-sm font-medium text-muted-foreground">系统状态</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">
              {status?.status === 'healthy' ? '健康' : status?.status === 'degraded' ? '降级' : '未知'}
            </div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="pb-2">
            <CardTitle className="text-sm font-medium text-muted-foreground">文档数量</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{status?.total_documents ?? 0}</div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="pb-2">
            <CardTitle className="text-sm font-medium text-muted-foreground">分块数量</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{status?.total_chunks ?? 0}</div>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="pb-2">
            <CardTitle className="text-sm font-medium text-muted-foreground">查询次数</CardTitle>
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{status?.total_queries ?? 0}</div>
          </CardContent>
        </Card>
      </div>

      <Card>
        <CardHeader>
          <CardTitle>系统详情</CardTitle>
        </CardHeader>
        <CardContent>
          <div className="space-y-2">
            <div className="flex justify-between">
              <span className="text-muted-foreground">运行时间</span>
              <span>{status?.uptime_seconds ? `${Math.floor(status.uptime_seconds / 60)} 分钟` : 'N/A'}</span>
            </div>
            <div className="flex justify-between">
              <span className="text-muted-foreground">活跃管道</span>
              <span>{status?.active_pipelines?.join(', ') || 'N/A'}</span>
            </div>
            <div className="flex justify-between">
              <span className="text-muted-foreground">已加载模型</span>
              <span>{status?.models_loaded?.join(', ') || 'N/A'}</span>
            </div>
          </div>
        </CardContent>
      </Card>
    </div>
  );
}