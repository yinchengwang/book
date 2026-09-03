import { useRef, useCallback } from 'react'

interface Turn {
  user: string
  assistant: string
}

// 多轮对话上下文管理（设计文档 3.2.4：前端拼接 prompt 方案）
export const useContext = (maxTurns: number) => {
  const historyRef = useRef<Turn[]>([])

  // 记录一轮完整对话（在收到 AI 回复后调用）
  const recordTurn = useCallback((user: string, assistant: string) => {
    historyRef.current.push({ user, assistant })
    if (historyRef.current.length > maxTurns) {
      historyRef.current = historyRef.current.slice(-maxTurns)
    }
  }, [maxTurns])

  // 构建带上下文的查询
  const buildQuery = useCallback((currentQuery: string): string => {
    const history = historyRef.current.slice(-maxTurns)
    if (maxTurns <= 0 || history.length === 0) {
      return currentQuery
    }

    const context = history
      .map(t => `用户: ${t.user}\n助手: ${t.assistant}`)
      .join('\n')

    return `以下是对话历史:\n${context}\n\n用户当前问题: ${currentQuery}\n\n请基于对话历史和当前问题提供回答。`
  }, [maxTurns])

  const clearContext = useCallback(() => {
    historyRef.current = []
  }, [])

  return { buildQuery, recordTurn, clearContext }
}
