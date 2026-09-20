import { useState } from 'react'
import { useMutation, useQueryClient } from '@tanstack/react-query'
import { FolderPlus, Trash2 } from 'lucide-react'
import { Button } from './ui/button'
import { Dialog, DialogContent, DialogHeader, DialogTitle } from './ui/dialog'

const API = '/api/v1'

interface NewFolderDialogProps {
  open: boolean
  onClose: () => void
  currentDir: string
}

export function NewFolderDialog({ open, onClose, currentDir }: NewFolderDialogProps) {
  const [name, setName] = useState('')
  const [error, setError] = useState('')
  const qc = useQueryClient()

  const createMutation = useMutation({
    mutationFn: async (path: string) => {
      const res = await fetch(`${API}/documents/dirs`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ path }),
      })
      const data = await res.json()
      if (!res.ok) throw new Error(data.error || '创建失败')
      return data
    },
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ['dirs'] })
      qc.invalidateQueries({ queryKey: ['documents'] })
      handleClose()
    },
    onError: (err: any) => setError(err.message),
  })

  const handleClose = () => {
    setName('')
    setError('')
    onClose()
  }

  const handleCreate = () => {
    const path = currentDir
      ? `${currentDir}/${name.trim()}`
      : name.trim()
    if (!path.trim()) return
    createMutation.mutate(path)
  }

  return (
    <Dialog open={open} onOpenChange={handleClose}>
      <DialogContent className="max-w-sm">
        <DialogHeader>
          <DialogTitle className="flex items-center gap-2">
            <FolderPlus className="w-5 h-5" />
            新建文件夹
          </DialogTitle>
        </DialogHeader>

        {currentDir && (
          <p className="text-sm text-gray-500 dark:text-gray-400">
            当前目录: <span className="font-mono">{currentDir}/</span>
          </p>
        )}

        <input
          type="text"
          value={name}
          onChange={e => { setName(e.target.value); setError('') }}
          onKeyDown={e => e.key === 'Enter' && handleCreate()}
          placeholder="文件夹名称"
          autoFocus
          className="w-full px-3 py-2 border rounded dark:bg-gray-800 dark:border-gray-600 dark:text-gray-100"
        />

        {error && (
          <p className="text-sm text-red-500">{error}</p>
        )}

        <div className="flex justify-end gap-2">
          <Button variant="outline" onClick={handleClose}>取消</Button>
          <Button
            onClick={handleCreate}
            disabled={!name.trim() || createMutation.isPending}
          >
            {createMutation.isPending ? '创建中...' : '创建'}
          </Button>
        </div>
      </DialogContent>
    </Dialog>
  )
}

interface DeleteDirDialogProps {
  open: boolean
  onClose: () => void
  dirPath: string
}

export function DeleteDirDialog({ open, onClose, dirPath }: DeleteDirDialogProps) {
  const [error, setError] = useState('')
  const qc = useQueryClient()

  const deleteMutation = useMutation({
    mutationFn: async () => {
      const res = await fetch(`${API}/documents/dirs?path=${encodeURIComponent(dirPath)}`, {
        method: 'DELETE',
      })
      const data = await res.json()
      if (!res.ok && !data.deleted) throw new Error(data.error || '删除失败')
      return data
    },
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ['dirs'] })
      qc.invalidateQueries({ queryKey: ['documents'] })
      handleClose()
    },
    onError: (err: any) => setError(err.message),
  })

  const handleClose = () => {
    setError('')
    onClose()
  }

  return (
    <Dialog open={open} onOpenChange={handleClose}>
      <DialogContent className="max-w-sm">
        <DialogHeader>
          <DialogTitle className="flex items-center gap-2 text-red-600">
            <Trash2 className="w-5 h-5" />
            删除文件夹
          </DialogTitle>
        </DialogHeader>

        <p className="text-sm dark:text-gray-300">
          确定删除文件夹 <span className="font-mono font-bold">{dirPath}</span> ？
        </p>
        <p className="text-xs text-gray-500">文件夹必须为空才能删除</p>

        {error && (
          <p className="text-sm text-red-500">{error}</p>
        )}

        <div className="flex justify-end gap-2">
          <Button variant="outline" onClick={handleClose}>取消</Button>
          <Button
            variant="destructive"
            onClick={() => deleteMutation.mutate()}
            disabled={deleteMutation.isPending}
          >
            {deleteMutation.isPending ? '删除中...' : '删除'}
          </Button>
        </div>
      </DialogContent>
    </Dialog>
  )
}
