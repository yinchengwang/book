import { useState, useCallback } from 'react'
import type { DocumentContent } from '@/types'

export const useDocument = () => {
  const [previewDocument, setPreviewDocument] = useState<DocumentContent | null>(null)
  const [isLoading, setIsLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)

  // id 可以是 document_id 或 file_path（后端两种都支持匹配）
  const loadDocument = useCallback(async (id: string) => {
    setIsLoading(true)
    setError(null)

    try {
      const response = await fetch(`/api/v1/documents/${encodeURIComponent(id)}/content`)
      if (!response.ok) {
        throw new Error(`加载文档失败: ${response.status} ${response.statusText}`)
      }
      const doc: DocumentContent = await response.json()
      setPreviewDocument(doc)
    } catch (err) {
      setError(err instanceof Error ? err.message : '加载文档失败')
    } finally {
      setIsLoading(false)
    }
  }, [])

  const closePreview = useCallback(() => {
    setPreviewDocument(null)
    setError(null)
  }, [])

  return {
    previewDocument,
    isLoading,
    error,
    loadDocument,
    closePreview
  }
}
