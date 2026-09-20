'use client';

import { useEffect, useState, useRef } from 'react';
import { Card, CardHeader, CardTitle, CardDescription, CardContent } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { Input } from '@/components/ui/input';
import { Upload, File, Trash2, Loader2, AlertCircle, Check, ChevronRight } from 'lucide-react';
import { formatBytes, formatTimestamp } from '@/lib/utils';
import { apiClient, type Document as ApiDoc, type UploadTask } from '@/lib/api';

interface UploadingFile {
  tempId: string;
  name: string;
  size: number;
  uploaded_at: number;
  // 进度状态
  task_id?: string;
  stage: 'queued' | 'uploading' | 'parsing' | 'chunking' | 'embedding' | 'indexing' | 'completed' | 'failed';
  progress: number;
  processed: number;
  total: number;
  elapsed_ms: number;
  error?: string;
}

// 上传阶段定义 — 按时间顺序
const STAGES = [
  { key: 'uploading', label: '上传文件', icon: Upload },
  { key: 'queued',    label: '等待处理', icon: Loader2 },
  { key: 'parsing',   label: '解析文档', icon: File },
  { key: 'chunking',  label: '文本分块', icon: ChevronRight },
  { key: 'embedding', label: '生成向量', icon: ChevronRight },
  { key: 'indexing',  label: '建立索引', icon: ChevronRight },
  { key: 'completed', label: '完成',     icon: Check },
] as const;

const STAGE_INDEX: Record<string, number> = {
  uploading: 0, queued: 1, parsing: 2, chunking: 3, embedding: 4, indexing: 5, completed: 6,
};

export function KnowledgeView() {
  const [documents, setDocuments] = useState<ApiDoc[]>([]);
  const [uploading, setUploading] = useState<UploadingFile[]>([]);
  const [dragOver, setDragOver] = useState(false);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  // 用 ref 跟踪轮询定时器，组件卸载时清理
  const pollRefs = useRef<Map<string, ReturnType<typeof setInterval>>>(new Map());

  // 初始加载 + 上传/删除后刷新
  const refresh = async () => {
    try {
      const docs = await apiClient.listDocuments();
      setDocuments(docs);
      setError(null);
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    refresh();
  }, []);

  // 组件卸载时清理所有轮询
  useEffect(() => {
    const refs = pollRefs.current;
    return () => {
      refs.forEach(t => clearInterval(t));
      refs.clear();
    };
  }, []);

  const startPolling = (tempId: string, taskId: string) => {
    const timer = setInterval(async () => {
      try {
        const task: UploadTask = await apiClient.getUploadProgress(taskId);
        setUploading((prev) =>
          prev.map((u) =>
            u.tempId === tempId
              ? {
                  ...u,
                  task_id: taskId,
                  stage: task.stage,
                  progress: task.progress,
                  processed: task.processed,
                  total: task.total,
                  elapsed_ms: task.elapsed_ms,
                  error: task.error,
                }
              : u
          )
        );

        // 完成后清理
        if (task.completed || task.stage === 'failed') {
          const t = pollRefs.current.get(tempId);
          if (t) clearInterval(t);
          pollRefs.current.delete(tempId);
          // 完成后刷新文档列表
          if (task.completed) {
            refresh();
          }
        }
      } catch (e) {
        // 详细日志：便于排查轮询失败原因（CORS、网络、服务器等）
        console.error('[Upload progress polling error]', {
          taskId,
          error: (e as Error).message,
          hint: '可能是 CORS 问题或后端未启动',
        });
      }
    }, 400);
    pollRefs.current.set(tempId, timer);
  };

  const handleUpload = async (files: FileList | null) => {
    if (!files || files.length === 0) return;

    for (const file of Array.from(files)) {
      const tempId = `up_${Date.now()}_${Math.random().toString(36).slice(2, 9)}`;
      const entry: UploadingFile = {
        tempId,
        name: file.name,
        size: file.size,
        uploaded_at: Date.now(),
        stage: 'uploading',  // 初始状态：浏览器 HTTP 上传中
        progress: 0,
        processed: 0,
        total: file.size,  // 总大小固定为文件大小
        elapsed_ms: 0,
      };
      setUploading((prev) => [entry, ...prev]);

      try {
        // 1. 上传文件（含上传进度回调）
        const task = await apiClient.uploadDocument(file, (loaded, total) => {
          // 实时更新前端显示的上传进度
          const uploadProgress = Math.round((loaded / total) * 100);
          // 当 HTTP body 全部发送完毕但服务器还没响应时，
          // 主动切到 'queued' 状态，避免视觉上卡在 'uploading 100%'
          const newStage = loaded >= total ? 'queued' : 'uploading';
          setUploading((prev) =>
            prev.map((u) =>
              u.tempId === tempId
                ? {
                    ...u,
                    stage: newStage as const,
                    progress: uploadProgress,
                    processed: loaded,
                    total: total,
                  }
                : u
            )
          );
        });
        // 上传完成 → 立刻把阶段切到 'queued'（避免卡在 uploading 视觉上）
        setUploading((prev) =>
          prev.map((u) =>
            u.tempId === tempId
              ? { ...u, stage: 'queued' as const }
              : u
          )
        );
        // 2. 开始轮询后端进度（400ms 后第一帧会拿到真实的 parsing/embedding 阶段）
        startPolling(tempId, task.task_id);
      } catch (e) {
        setUploading((prev) =>
          prev.map((u) =>
            u.tempId === tempId
              ? { ...u, stage: 'failed' as const, error: (e as Error).message }
              : u
          )
        );
      }
    }
  };

  const handleDelete = async (id: string) => {
    try {
      await apiClient.deleteDocument(id);
      await refresh();
    } catch (e) {
      setError((e as Error).message);
    }
  };

  return (
    <div className="p-6 space-y-6 overflow-y-auto h-full">
      <div>
        <h2 className="text-2xl font-bold">知识库管理</h2>
        <p className="text-sm text-muted-foreground">
          上传文档以构建 RAG 知识库，支持 PDF、Markdown、Word 等格式
        </p>
      </div>

      {error && (
        <Card>
          <CardContent className="pt-6">
            <div className="flex items-center gap-2 text-destructive">
              <AlertCircle className="h-4 w-4" />
              <span className="text-sm">加载文档列表失败：{error}</span>
            </div>
          </CardContent>
        </Card>
      )}

      {/* 上传区域 */}
      <Card>
        <CardHeader>
          <CardTitle>上传文档</CardTitle>
          <CardDescription>拖放文件到此处或点击选择</CardDescription>
        </CardHeader>
        <CardContent>
          <div
            onDragOver={(e) => {
              e.preventDefault();
              setDragOver(true);
            }}
            onDragLeave={() => setDragOver(false)}
            onDrop={(e) => {
              e.preventDefault();
              setDragOver(false);
              handleUpload(e.dataTransfer.files);
            }}
            className={`border-2 border-dashed rounded-lg p-8 text-center transition-colors cursor-pointer ${
              dragOver ? 'border-primary bg-primary/5' : 'border-muted'
            }`}
            onClick={() => document.getElementById('file-input')?.click()}
          >
            <Upload className="mx-auto h-12 w-12 text-muted-foreground mb-3" />
            <p className="text-sm font-medium">
              {uploading.some((u) => u.stage !== 'completed' && u.stage !== 'failed')
                ? '上传中...'
                : '拖放文件或点击上传'}
            </p>
            <p className="text-xs text-muted-foreground mt-1">
              支持 PDF、Markdown、Word、TXT 等格式
            </p>
            <input
              id="file-input"
              type="file"
              multiple
              hidden
              onChange={(e) => handleUpload(e.target.files)}
            />
          </div>
        </CardContent>
      </Card>

      {/* 正在上传（带阶段进度） */}
      {uploading.length > 0 && (
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Loader2 className="h-4 w-4 animate-spin" />
              上传中 ({uploading.filter((u) => u.stage !== 'completed' && u.stage !== 'failed').length})
            </CardTitle>
          </CardHeader>
          <CardContent>
            <div className="space-y-3">
              {uploading.map((u) => {
                const currentStageIdx = STAGE_INDEX[u.stage] ?? 0;
                const isFailed = u.stage === 'failed';
                const isCompleted = u.stage === 'completed';

                return (
                  <div key={u.tempId} className="p-4 rounded-lg border bg-card">
                    {/* 文件信息 + 总体进度 */}
                    <div className="flex items-center justify-between mb-3">
                      <div className="flex items-center gap-2 flex-1 min-w-0">
                        <File className="h-4 w-4 text-muted-foreground flex-shrink-0" />
                        <span className="font-medium truncate">{u.name}</span>
                        <span className="text-xs text-muted-foreground flex-shrink-0">
                          {formatBytes(u.size)}
                        </span>
                      </div>
                      <div className="flex items-center gap-2">
                        {isFailed && <Badge variant="destructive">失败</Badge>}
                        {isCompleted && (
                          <Badge variant="success">
                            <Check className="h-3 w-3 mr-1" />
                            完成
                          </Badge>
                        )}
                        {!isFailed && !isCompleted && (
                          <Badge variant="outline">
                            {Math.round(u.progress)}%
                          </Badge>
                        )}
                      </div>
                    </div>

                    {/* 阶段进度条 (xxx >> xxx >> xxx) */}
                    <div className="flex items-center gap-1 mb-3 text-xs overflow-x-auto">
                      {STAGES.map((stage, idx) => {
                        const isPast = idx < currentStageIdx || isCompleted;
                        const isCurrent = idx === currentStageIdx && !isCompleted && !isFailed;
                        const Icon = stage.icon;
                        return (
                          <div key={stage.key} className="flex items-center gap-1 flex-shrink-0">
                            <div
                              className={`flex items-center gap-1 px-2 py-1 rounded transition-colors ${
                                isPast
                                  ? 'bg-green-500/20 text-green-700 dark:text-green-300'
                                  : isCurrent
                                  ? 'bg-blue-500/20 text-blue-700 dark:text-blue-300 font-medium'
                                  : 'bg-muted text-muted-foreground'
                              }`}
                            >
                              {isPast ? (
                                <Check className="h-3 w-3" />
                              ) : isCurrent ? (
                                <Loader2 className="h-3 w-3 animate-spin" />
                              ) : (
                                <Icon className="h-3 w-3" />
                              )}
                              <span>{stage.label}</span>
                            </div>
                            {idx < STAGES.length - 1 && (
                              <ChevronRight className="h-3 w-3 text-muted-foreground flex-shrink-0" />
                            )}
                          </div>
                        );
                      })}
                    </div>

                    {/* 详细信息 */}
                    {!isFailed && !isCompleted && (
                      <div className="space-y-2">
                        {/* 当前阶段进度条 */}
                        <div className="w-full bg-muted rounded-full h-1.5 overflow-hidden">
                          <div
                            className="bg-primary h-full transition-all duration-300"
                            style={{ width: `${u.progress}%` }}
                          />
                        </div>
                        {/* 详细计数 */}
                        <div className="flex justify-between text-xs text-muted-foreground">
                          <span>
                            {u.stage === 'uploading' && u.total > 0
                              ? `已上传 ${formatBytes(u.processed)} / ${formatBytes(u.total)}`
                              : u.stage === 'queued'
                              ? '等待后端开始处理...'
                              : u.stage === 'parsing'
                              ? '解析文档内容...'
                              : u.stage === 'chunking'
                              ? `文本分块：${u.processed} / ${u.total}`
                              : u.stage === 'embedding' || u.stage === 'indexing'
                              ? `${u.stage === 'embedding' ? '生成向量' : '建立索引'}：${u.processed} / ${u.total} 块`
                              : '处理中...'}
                          </span>
                          <span>{(u.elapsed_ms / 1000).toFixed(1)}s</span>
                        </div>
                      </div>
                    )}

                    {isCompleted && (
                      <div className="text-xs text-muted-foreground">
                        处理完成 · 共 {(u.elapsed_ms / 1000).toFixed(1)}s
                      </div>
                    )}

                    {isFailed && u.error && (
                      <div className="text-xs text-destructive">
                        {u.error}
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
          </CardContent>
        </Card>
      )}

      {/* 文档列表 */}
      <Card>
        <CardHeader>
          <CardTitle>文档列表 ({documents.length})</CardTitle>
        </CardHeader>
        <CardContent>
          {loading ? (
            <div className="text-center text-muted-foreground py-8">加载中...</div>
          ) : documents.length === 0 ? (
            <div className="text-center text-muted-foreground py-8">
              还没有上传任何文档
            </div>
          ) : (
            <div className="space-y-2">
              {documents.map((doc) => (
                <div
                  key={doc.id}
                  className="flex items-center justify-between p-3 rounded border hover:bg-accent/50 transition-colors"
                >
                  <div className="flex items-center gap-3 flex-1 min-w-0">
                    <File className="h-5 w-5 text-muted-foreground flex-shrink-0" />
                    <div className="flex-1 min-w-0">
                      <div className="font-medium truncate">{doc.name}</div>
                      <div className="text-xs text-muted-foreground">
                        {formatBytes(doc.size)} · {formatTimestamp(doc.uploaded_at)}
                        {doc.chunk_count !== undefined && ` · ${doc.chunk_count} 个分块`}
                        {doc.error_message && ` · 错误：${doc.error_message}`}
                      </div>
                    </div>
                  </div>

                  <div className="flex items-center gap-2">
                    {doc.status === 'pending' && (
                      <Badge variant="secondary">等待中</Badge>
                    )}
                    {doc.status === 'processing' && (
                      <Badge variant="warning">
                        <Loader2 className="h-3 w-3 mr-1 animate-spin" />
                        处理中
                      </Badge>
                    )}
                    {doc.status === 'indexed' && <Badge variant="success">已索引</Badge>}
                    {doc.status === 'failed' && <Badge variant="destructive">失败</Badge>}

                    <Button
                      variant="ghost"
                      size="icon"
                      onClick={() => handleDelete(doc.id)}
                    >
                      <Trash2 className="h-4 w-4" />
                    </Button>
                  </div>
                </div>
              ))}
            </div>
          )}
        </CardContent>
      </Card>
    </div>
  );
}