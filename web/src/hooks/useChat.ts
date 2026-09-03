import { useState, useCallback, useEffect } from 'react'
import type { Message } from '@/types'
import { generateId } from '@/lib/utils'

const STORAGE_KEY = 'rag-chat-history'
const MAX_PERSISTED = 100  // 最多持久化 100 条消息

export const useChat = () => {
  const [messages, setMessages] = useState<Message[]>(() => {
    try {
      const saved = localStorage.getItem(STORAGE_KEY)
      return saved ? JSON.parse(saved) : []
    } catch {
      return []
    }
  })
  const [isLoading, setIsLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)

  // 持久化到 localStorage
  useEffect(() => {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(messages.slice(-MAX_PERSISTED)))
    } catch {
      // 存储满时静默失败
    }
  }, [messages])

  const addMessage = useCallback((message: Omit<Message, 'id' | 'timestamp'>) => {
    const newMessage: Message = {
      ...message,
      id: generateId(),
      timestamp: Date.now()
    }
    setMessages(prev => [...prev, newMessage])
    return newMessage
  }, [])

  const clearMessages = useCallback(() => {
    setMessages([])
    setError(null)
    localStorage.removeItem(STORAGE_KEY)
  }, [])

  return {
    messages,
    isLoading,
    error,
    addMessage,
    clearMessages,
    setIsLoading,
    setError
  }
}