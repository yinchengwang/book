import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { RefreshCw, CheckCircle, AlertCircle } from 'lucide-react'
import { Button } from './ui/button'
import { Dialog, DialogContent, DialogHeader, DialogTitle } from './ui/dialog'
import type { IndexStatus } from '../types'

const API = '/api/v1'

interface RebuildDialogProps {
  open: boolean
  onClose: () => void
  currentDir?: string
}

type RebuildStatus = 'idle' | 'started' | 'running' | 'completed' | 'failed'

interface RebuildJob {
  job_id: string
  status: RebuildStatus
  progress: number
  current_file?: string
  message?: string
}

export function RebuildDialog({ open, onClose, currentDir }: RebuildDialogProps) {
  const [job, setJob] = useState<RebuildJob | null>(null)
  const [polling, setPolling] = useState(false)

  // 索引状态
  const { data: indexStatus } = useQuery<IndexStatus>({
    queryKey: ['indexStatus'],
    queryFn: async () => {
      const res = await fetch(`${API}/index/status`)
      if (!res.ok) throw new Error('Failed')
      return res.json()
    },
  })

  const startRebuild = async () => {
    try {
      const res = await fetch(`${API}/index/rebuild`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ force: true, dir: currentDir || '' }),
      })
      if (!res.ok) throw new Error('Failed to start rebuild')
      const data = await res.json()
      setJob({ job_id: data.job_id, status: 'started', progress: 0 })
      setPolling(true)
    } catch {
      setJob({ job_id: '', status: 'failed', progress: 0, message: '启动重建失败' })
      setPolling(false)
    }
  }

  // 轮询状态
  const { } = useQuery({
    queryKey: ['rebuildStatus', job?.job_id],
    queryFn: async () => {
      if (!job?.job_id) return null
      const res = await fetch(`${API}/index/rebuild/status?job_id=${job.job_id}`)
      if (!res.ok) return null
      const data = await res.json()
      const newStatus: RebuildStatus = data.status === 'completed' ? 'completed'
        : data.status === 'failed' ? 'failed'
        : data.status === 'running' ? 'running'
        : 'started'
      setJob(prev => prev ? { ...prev, ...data, status: newStatus } : null)
      if (newStatus === 'completed' || newStatus === 'failed') {
        setPolling(false)
      }
      return data
    },
    enabled: polling && !!job?.job_id,
    refetchInterval: 2000,
  })

  const handleClose = () => {
    setJob(null)
    setPolling(false)
    onClose()
  }

  const progress = job?.progress ?? 0
  const statusLabel: Record<RebuildStatus, string> = {
    idle: '就绪',
    started: '启动中',
    running: '重建中',
    completed: '已完成',
    failed: '失败',
  }

  return (
    <Dialog open={open} onOpenChange={handleClose}>
      <DialogContent className="max-w-md">
        <DialogHeader>
          <DialogTitle className="flex items-center gap-2">
            <RefreshCw className="w-5 h-5" />
            重建索引
          </DialogTitle>
        </DialogHeader>

        {/* 当前索引状态 */}
        {indexStatus && (
          <div className="bg-gray-50 dark:bg-gray-800 rounded p-3 text-sm space-y-1">
            <div className="flex justify-between">
              <span className="text-gray-500">文档数</span>
              <span className="font-medium dark:text-gray-100">{indexStatus.document_count}</span>
            </div>
            <div className="flex justify-between">
              <span className="text-gray-500">块数</span>
              <span className="font-medium dark:text-gray-100">{indexStatus.chunk_count}</span>
            </div>
            <div className="flex justify-between">
              <span className="text-gray-500">向量数</span>
              <span className="font-medium dark:text-gray-100">{indexStatus.vector_count}</span>
            </div>
          </div>
        )}

        {/* 重建进度 */}
        {job && job.status !== 'idle' && (
          <div className="space-y-2">
            <div className="flex items-center justify-between text-sm">
              <span className="text-gray-600 dark:text-gray-300">{statusLabel[job.status]}</span>
              <span className="font-medium dark:text-gray-100">{(progress * 100).toFixed(0)}%</span>
            </div>
            <div className="w-full bg-gray-200 dark:bg-gray-700 rounded-full h-2">
              <div
                className="bg-blue-500 h-2 rounded-full transition-all duration-500"
                style={{ width: `${progress * 100}%` }}
              />
            </div>
            {job.current_file && (
              <p className="text-xs text-gray-400 truncate">
                正在处理: {job.current_file}
              </p>
            )}
            {job.message && (
              <p className="text-xs text-gray-500">{job.message}</p>
            )}
            {job.status === 'completed' && (
              <div className="flex items-center gap-2 text-green-600 text-sm">
                <CheckCircle className="w-4 h-4" />
                索引重建完成
              </div>
            )}
            {job.status === 'failed' && (
              <div className="flex items-center gap-2 text-red-600 text-sm">
                <AlertCircle className="w-4 h-4" />
                索引重建失败
              </div>
            )}
          </div>
        )}

        <div className="flex justify-end gap-2 pt-2">
          <Button variant="outline" onClick={handleClose}>关闭</Button>
          {!job || job.status === 'idle' || job.status === 'completed' || job.status === 'failed' ? (
            <Button onClick={startRebuild} disabled={job?.status === 'started'}>
              <RefreshCw className="w-4 h-4 mr-1" />
              开始重建
            </Button>
          ) : null}
        </div>
      </DialogContent>
    </Dialog>
  )
}
