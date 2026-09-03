import { useCallback } from 'react'
import { useChat } from '@/hooks/useChat'
import { useRAG } from '@/hooks/useRAG'
import { MessageList } from './MessageList'
import { InputBox } from './InputBox'
import type { QueryOptions, ChunkReference } from '@/types'

interface ChatProps {
  options: QueryOptions
  buildQuery?: (content: string) => string  // Task 11 注入上下文拼接
  onChunkClick?: (chunk: ChunkReference) => void
  onAssistantMessage?: (content: string) => void  // Task 11 记录上下文
}

export const Chat = ({ options, buildQuery, onChunkClick, onAssistantMessage }: ChatProps) => {
  const { messages, isLoading, error, addMessage, setIsLoading, setError } = useChat()
  const { query } = useRAG()

  const handleSend = useCallback(async (content: string) => {
    addMessage({ role: 'user', content })

    setIsLoading(true)
    setError(null)

    try {
      const queryText = buildQuery ? buildQuery(content) : content
      const response = await query(queryText, options)

      addMessage({
        role: 'assistant',
        content: response.answer,
        chunks: response.chunks
      })
      onAssistantMessage?.(response.answer)
    } catch (err) {
      setError(err instanceof Error ? err.message : '查询失败')
    } finally {
      setIsLoading(false)
    }
  }, [addMessage, query, options, buildQuery, onAssistantMessage, setIsLoading, setError])

  return (
    <div className="flex flex-col h-full">
      {error && (
        <div className="bg-red-100 text-red-700 px-4 py-2 text-sm dark:bg-red-900/20 dark:text-red-400">
          {error}
        </div>
      )}

      <MessageList
        messages={messages}
        isLoading={isLoading}
        onChunkClick={onChunkClick}
      />

      <InputBox onSend={handleSend} isLoading={isLoading} />
    </div>
  )
}