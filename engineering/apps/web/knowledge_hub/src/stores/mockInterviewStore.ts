import { create } from 'zustand'
import { persist } from 'zustand/middleware'

// 面试题目接口
export interface InterviewQuestion {
  question: string
  answer: string
  evaluation?: {
    score: number
    feedback: string
    suggestions: string[]
  }
}

// 面试会话接口
export interface MockSession {
  id: string
  date: string
  role: string
  difficulty: string
  questions: InterviewQuestion[]
  totalScore: number
  duration?: number
}

// Store 接口
interface MockInterviewState {
  sessionHistory: MockSession[]
  currentSession: MockSession | null

  addSession: (session: MockSession) => void
  deleteSession: (id: string) => void
  clearHistory: () => void
  getSessionById: (id: string) => MockSession | undefined
}

// 初始示例数据
const initialSessions: MockSession[] = [
  {
    id: 'session-1',
    date: new Date(Date.now() - 86400000).toISOString(), // 昨天
    role: '后端开发',
    difficulty: '中级',
    questions: [
      {
        question: '请实现一个 LRU 缓存',
        answer: '使用哈希表+双向链表实现...',
        evaluation: {
          score: 85,
          feedback: '回答完整，思路清晰',
          suggestions: ['可以补充并发安全的实现']
        }
      }
    ],
    totalScore: 85,
    duration: 25
  }
]

export const useMockInterviewStore = create<MockInterviewState>()(
  persist(
    (set, get) => ({
      sessionHistory: initialSessions,
      currentSession: null,

      // 添加会话
      addSession: (session) => set((state) => ({
        sessionHistory: [...state.sessionHistory, session],
        currentSession: session
      })),

      // 删除会话
      deleteSession: (id) => set((state) => ({
        sessionHistory: state.sessionHistory.filter(s => s.id !== id)
      })),

      // 清空历史
      clearHistory: () => set({ sessionHistory: [] }),

      // 获取单个会话
      getSessionById: (id) => get().sessionHistory.find(s => s.id === id)
    }),
    {
      name: 'mock-interview-storage'
    }
  )
)
