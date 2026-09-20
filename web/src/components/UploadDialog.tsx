import { useRef, useState, useCallback } from 'react'
import { useQueryClient } from '@tanstack/react-query'
import { Upload, File, CheckCircle, AlertCircle, Loader2 } from 'lucide-react'
import { Button } from './ui/button'
import { Dialog, DialogContent, DialogHeader, DialogTitle } from './ui/dialog'

interface UploadFile {
  name: string
  size: number
  status: 'pending' | 'uploading' | 'done' | 'error'
  error?: string
  file: File
}

interface UploadDialogProps {
  open: boolean
  onClose: () => void
  currentDir: string
}

const API = '/api/v1'

export function UploadDialog({ open, onClose, currentDir }: UploadDialogProps) {
  const [files, setFiles] = useState<UploadFile[]>([])
  const [targetDir, setTargetDir] = useState('')
  const [isUploading, setIsUploading] = useState(false)
  const fileInputRef = useRef<HTMLInputElement>(null)
  const qc = useQueryClient()

  const handleFileSelect = useCallback((e: React.ChangeEvent<HTMLInputElement>) => {
    const selected = Array.from(e.target.files || [])
    setFiles(prev => [
      ...prev,
      ...selected.map(f => ({
        name: f.name,
        size: f.size,
        status: 'pending' as const,
        file: f,
      })),
    ])
    if (fileInputRef.current) fileInputRef.current.value = ''
  }, [])

  const handleDrop = useCallback((e: React.DragEvent) => {
    e.preventDefault()
    const dropped = Array.from(e.dataTransfer.files)
    setFiles(prev => [
      ...prev,
      ...dropped.map(f => ({
        name: f.name,
        size: f.size,
        status: 'pending' as const,
        file: f,
      })),
    ])
  }, [])

  const removeFile = (idx: number) => {
    setFiles(prev => prev.filter((_, i) => i !== idx))
  }

  const uploadAll = async () => {
    const pending = files.filter(f => f.status === 'pending')
    if (pending.length === 0) return
    setIsUploading(true)

    for (let i = 0; i < files.length; i++) {
      if (files[i].status !== 'pending') continue

      const fileObj = files[i].file
      setFiles(prev => prev.map((f, idx) =>
        idx === i ? { ...f, status: 'uploading' as const } : f
      ))

      try {
        const formData = new FormData()
        formData.append('file', fileObj)
        if (targetDir) formData.append('target_dir', targetDir)

        const res = await fetch(`${API}/documents`, {
          method: 'POST',
          body: formData,
        })

        if (res.ok) {
          setFiles(prev => prev.map((f, idx) =>
            idx === i ? { ...f, status: 'done' as const } : f
          ))
        } else {
          throw new Error(`HTTP ${res.status}`)
        }
      } catch (err: any) {
        setFiles(prev => prev.map((f, idx) =>
          idx === i ? { ...f, status: 'error' as const, error: err.message } : f
        ))
      }
    }

    setIsUploading(false)
    qc.invalidateQueries({ queryKey: ['documents'] })
    qc.invalidateQueries({ queryKey: ['dirs'] })
  }

  const handleClose = () => {
    setFiles([])
    setTargetDir('')
    setIsUploading(false)
    onClose()
  }

  const formatSize = (bytes: number) => {
    if (bytes < 1024) return `${bytes}B`
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)}KB`
    return `${(bytes / 1024 / 1024).toFixed(1)}MB`
  }

  const doneCount = files.filter(f => f.status === 'done').length
  const pendingCount = files.filter(f => f.status === 'pending').length

  return (
    <Dialog open={open} onOpenChange={handleClose}>
      <DialogContent className="max-w-lg">
        <DialogHeader>
          <DialogTitle className="flex items-center gap-2">
            <Upload className="w-5 h-5" />
            上传文件
          </DialogTitle>
        </DialogHeader>

        {/* 拖拽区域 */}
        <div
          className="border-2 border-dashed border-gray-300 dark:border-gray-600 rounded-lg p-6 text-center cursor-pointer hover:border-blue-500 dark:hover:border-blue-400 transition-colors"
          onDragOver={e => e.preventDefault()}
          onDrop={handleDrop}
          onClick={() => fileInputRef.current?.click()}
        >
          <Upload className="w-8 h-8 mx-auto mb-2 text-gray-400" />
          <p className="text-sm text-gray-500 dark:text-gray-400">
            拖拽文件到这里，或点击选择
          </p>
          <p className="text-xs text-gray-400 mt-1">
            支持 .md .txt .pdf 等格式
          </p>
          <input
            ref={fileInputRef}
            type="file"
            multiple
            className="hidden"
            onChange={handleFileSelect}
          />
        </div>

        {/* 目标目录 */}
        <div className="flex items-center gap-2">
          <label className="text-sm text-gray-600 dark:text-gray-300 whitespace-nowrap">
            保存到目录:
          </label>
          <input
            type="text"
            value={targetDir}
            onChange={e => setTargetDir(e.target.value)}
            placeholder={currentDir || '根目录'}
            className="flex-1 px-3 py-1.5 text-sm border rounded bg-white dark:bg-gray-800 dark:border-gray-600 dark:text-gray-100"
          />
        </div>

        {/* 文件列表 */}
        {files.length > 0 && (
          <div className="max-h-48 overflow-y-auto border rounded dark:border-gray-700">
            {files.map((f, i) => (
              <div key={i} className="flex items-center gap-2 px-3 py-2 border-b border-gray-100 dark:border-gray-700 last:border-0">
                <File className="w-4 h-4 text-gray-400 flex-shrink-0" />
                <span className="flex-1 text-sm truncate dark:text-gray-100">{f.name}</span>
                <span className="text-xs text-gray-400">{formatSize(f.size)}</span>
                {f.status === 'done' && <CheckCircle className="w-4 h-4 text-green-500" />}
                {f.status === 'error' && <AlertCircle className="w-4 h-4 text-red-500" />}
                {f.status === 'uploading' && <Loader2 className="w-4 h-4 animate-spin text-blue-500" />}
                {f.status === 'pending' && (
                  <button
                    className="text-gray-400 hover:text-red-500"
                    onClick={e => { e.stopPropagation(); removeFile(i); }}
                  >
                    ✕
                  </button>
                )}
              </div>
            ))}
          </div>
        )}

        {/* 进度信息 */}
        {files.length > 0 && (
          <div className="text-xs text-gray-400">
            {doneCount > 0 && <span className="text-green-600">{doneCount} 个已上传</span>}
            {pendingCount > 0 && <span className="ml-2">{pendingCount} 个待上传</span>}
          </div>
        )}

        {/* 操作按钮 */}
        <div className="flex justify-end gap-2">
          <Button variant="outline" onClick={handleClose} disabled={isUploading}>
            关闭
          </Button>
          <Button
            onClick={uploadAll}
            disabled={files.length === 0 || isUploading || pendingCount === 0}
          >
            {isUploading ? <Loader2 className="w-4 h-4 mr-1 animate-spin" /> : null}
            {isUploading ? '上传中...' : `上传 ${pendingCount} 个文件`}
          </Button>
        </div>
      </DialogContent>
    </Dialog>
  )
}
