import { useQuery, useQueryClient } from '@tanstack/react-query'
import { useState } from 'react'
import { FolderOpen, ChevronRight, FileText, File, Trash2, Loader2, RefreshCw, Upload } from 'lucide-react'
import { Button } from './ui/button'
import { Card } from './ui/card'
import type { DocumentsResponse, DocumentMeta, DirsResponse } from '../types'

const API = '/api/v1'

// 文件图标
function FileIcon({ type }: { type: string }) {
  if (type === 'pdf') return <File className="w-5 h-5 text-red-500" />
  if (type === 'md' || type === 'markdown') return <FileText className="w-5 h-5 text-blue-500" />
  if (type === 'txt' || type === 'text') return <FileText className="w-5 h-5 text-gray-500" />
  return <File className="w-5 h-5 text-gray-400" />
}

// 状态标签
function StatusBadge({ status }: { status: number }) {
  const map: Record<number, { label: string; cls: string }> = {
    0: { label: '待处理', cls: 'bg-gray-100 text-gray-600 dark:bg-gray-700 dark:text-gray-300' },
    1: { label: '处理中', cls: 'bg-yellow-100 text-yellow-700 dark:bg-yellow-900 dark:text-yellow-300' },
    2: { label: '已索引', cls: 'bg-green-100 text-green-700 dark:bg-green-900 dark:text-green-300' },
    3: { label: '失败', cls: 'bg-red-100 text-red-700 dark:bg-red-900 dark:text-red-300' },
  }
  const s = map[status] || map[0]
  return (
    <span className={`text-xs px-2 py-0.5 rounded-full ${s.cls}`}>{s.label}</span>
  )
}

// 格式化文件大小
function formatSize(bytes: number) {
  if (bytes < 1024) return `${bytes}B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)}KB`
  return `${(bytes / 1024 / 1024).toFixed(1)}MB`
}

// 格式化时间
function formatTime(ts: number) {
  if (!ts) return '-'
  return new Date(ts * 1000).toLocaleString('zh-CN')
}

// 文件行
function FileRow({ doc }: { doc: DocumentMeta }) {
  const [deleting, setDeleting] = useState(false)
  const qc = useQueryClient()

  const handleDelete = async () => {
    if (!confirm(`删除文档 "${doc.file_name}"？`)) return
    setDeleting(true)
    try {
      await fetch(`${API}/documents/${encodeURIComponent(doc.id)}`, { method: 'DELETE' })
      qc.invalidateQueries({ queryKey: ['documents'] })
    } finally {
      setDeleting(false)
    }
  }

  return (
    <div className="flex items-center gap-3 px-4 py-2 hover:bg-gray-50 dark:hover:bg-gray-800 group border-b border-gray-100 dark:border-gray-700 last:border-0">
      <FileIcon type={doc.file_type} />
      <div className="flex-1 min-w-0">
        <div className="text-sm font-medium truncate dark:text-gray-100">{doc.file_name}</div>
        <div className="text-xs text-gray-400">{formatSize(doc.file_size)}</div>
      </div>
      <div className="text-xs text-gray-400 hidden md:block">{formatTime(doc.indexed_at)}</div>
      <StatusBadge status={doc.status} />
      <Button
        variant="ghost"
        size="icon"
        className="opacity-0 group-hover:opacity-100 transition-opacity"
        onClick={handleDelete}
        disabled={deleting}
        title="删除"
      >
        {deleting ? <Loader2 className="w-4 h-4 animate-spin" /> : <Trash2 className="w-4 h-4 text-red-500" />}
      </Button>
    </div>
  )
}

// 面包屑导航
function Breadcrumb({ path, onNavigate }: { path: string[]; onNavigate: (idx: number) => void }) {
  return (
    <div className="flex items-center gap-1 text-sm text-gray-600 dark:text-gray-300 px-4 py-2 border-b border-gray-200 dark:border-gray-700 overflow-x-auto">
      <button
        className="hover:text-blue-600 dark:hover:text-blue-400"
        onClick={() => onNavigate(-1)}
      >
        全部
      </button>
      {path.map((seg, i) => (
        <span key={i} className="flex items-center gap-1">
          <ChevronRight className="w-3 h-3" />
          <button
            className="hover:text-blue-600 dark:hover:text-blue-400"
            onClick={() => onNavigate(i)}
          >
            {seg}
          </button>
        </span>
      ))}
    </div>
  )
}

// 目录行
function DirRow({ dir, onClick }: { dir: string; onClick: () => void }) {
  // 显示最后一层目录名
  const parts = dir.split('/')
  const name = parts[parts.length - 1]
  return (
    <button
      className="flex items-center gap-2 w-full px-4 py-2 hover:bg-gray-50 dark:hover:bg-gray-800 border-b border-gray-100 dark:border-gray-700 last:border-0 text-left"
      onClick={onClick}
    >
      <FolderOpen className="w-5 h-5 text-yellow-500" />
      <span className="text-sm dark:text-gray-100">{name}</span>
      <span className="text-xs text-gray-400 ml-auto">{dir}</span>
    </button>
  )
}

// 主组件
interface FileManagerProps {
  onUploadClick: () => void
  onRebuildClick: () => void
  onDirChange?: (dir: string) => void
}

export function FileManager({ onUploadClick, onRebuildClick, onDirChange }: FileManagerProps) {
  const [currentDir, setCurrentDir] = useState('')
  const queryClient = useQueryClient()

  // 监听目录变化，通知父组件
  const handleDirChange = (dir: string) => {
    setCurrentDir(dir)
    onDirChange?.(dir)
  }

  const dirPath = currentDir.split('/').filter(Boolean)

  // 文档列表
  const { data: docsData, isLoading: docsLoading } = useQuery<DocumentsResponse>({
    queryKey: ['documents', currentDir],
    queryFn: async () => {
      const url = currentDir
        ? `${API}/documents?dir=${encodeURIComponent(currentDir)}`
        : `${API}/documents`
      const res = await fetch(url)
      if (!res.ok) throw new Error('Failed to load documents')
      return res.json()
    },
  })

  // 子目录列表（从 dirs 响应中过滤当前目录的子级）
  const { data: dirsData } = useQuery<DirsResponse>({
    queryKey: ['dirs'],
    queryFn: async () => {
      const res = await fetch(`${API}/documents/dirs`)
      if (!res.ok) throw new Error('Failed to load dirs')
      return res.json()
    },
  })

  // 过滤当前目录的直接子目录
  const subDirs = dirsData?.dirs.filter(d => {
    if (!currentDir) {
      // 顶层目录：不含 '/' 的
      return !d.includes('/')
    }
    // 当前目录的下一级
    return d.startsWith(currentDir + '/') && !d.substring(currentDir.length + 1).includes('/')
  }) || []

  const navigateToDir = (idx: number) => {
    if (idx < 0) {
      handleDirChange('')
    } else {
      handleDirChange(dirPath.slice(0, idx + 1).join('/'))
    }
  }

  return (
    <Card className="h-full flex flex-col">
      {/* 工具栏 */}
      <div className="flex items-center gap-2 px-4 py-3 border-b border-gray-200 dark:border-gray-700">
        <Button size="sm" variant="outline" onClick={onUploadClick}>
          <Upload className="w-4 h-4 mr-1" />
          上传
        </Button>
        <Button size="sm" variant="outline" onClick={onRebuildClick}>
          <RefreshCw className="w-4 h-4 mr-1" />
          重建索引
        </Button>
        <Button
          size="sm"
          variant="ghost"
          onClick={() => queryClient.invalidateQueries({ queryKey: ['documents'] })}
        >
          <RefreshCw className="w-4 h-4" />
        </Button>
      </div>

      {/* 面包屑 */}
      <Breadcrumb path={dirPath} onNavigate={navigateToDir} />

      {/* 内容区 */}
      <div className="flex-1 overflow-y-auto">
        {docsLoading ? (
          <div className="flex items-center justify-center py-12">
            <Loader2 className="w-6 h-6 animate-spin text-gray-400" />
          </div>
        ) : (
          <>
            {/* 子目录 */}
            {subDirs.map(dir => (
              <DirRow
                key={dir}
                dir={dir}
                onClick={() => {
                  // 导航到该目录（最后一层）
                  const parts = dir.split('/')
                  const currentDepth = currentDir ? currentDir.split('/').filter(Boolean).length : 0
                  handleDirChange(parts.slice(0, currentDepth + 1).join('/'))
                }}
              />
            ))}

            {/* 文件列表 */}
            {docsData?.documents.map(doc => (
              <FileRow
                key={doc.id}
                doc={doc}
              />
            ))}

            {/* 空状态 */}
            {subDirs.length === 0 && (!docsData?.documents || docsData.documents.length === 0) && (
              <div className="flex flex-col items-center justify-center py-12 text-gray-400">
                <FolderOpen className="w-12 h-12 mb-3" />
                <p className="text-sm">暂无文件</p>
                <p className="text-xs mt-1">点击上方"上传"添加文档</p>
              </div>
            )}
          </>
        )}
      </div>

      {/* 底部统计 */}
      <div className="px-4 py-2 border-t border-gray-200 dark:border-gray-700 text-xs text-gray-400">
        {docsData && `${docsData.total} 个文件`}
        {dirsData && ` · ${dirsData.total} 个目录`}
      </div>
    </Card>
  )
}
