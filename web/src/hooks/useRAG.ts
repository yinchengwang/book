import { useCallback } from 'react'
import type { QueryOptions, QueryResponse } from '@/types'

export const useRAG = () => {
  const query = useCallback(async (
    queryText: string,
    options: QueryOptions
  ): Promise<QueryResponse> => {
    const response = await fetch('/api/v1/query', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        query: queryText,
        top_k: options.topK
      })
    })

    if (!response.ok) {
      throw new Error(`查询失败: ${response.status} ${response.statusText}`)
    }

    return response.json()
  }, [])

  const healthCheck = useCallback(async (): Promise<{ status: string }> => {
    const response = await fetch('/health')
    if (!response.ok) {
      throw new Error(`健康检查失败: ${response.statusText}`)
    }
    return response.json()
  }, [])

  return { query, healthCheck }
}
