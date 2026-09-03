import { create } from 'zustand'
import { persist } from 'zustand/middleware'
import { allQuizQuestions, quizByStack, quizStats } from '@/data/quiz-index'

// 题目接口
export interface QuizQuestion {
  id: string
  title: string
  description?: string
  category: '选择题' | '简答题' | '编程题' | '判断题' | '填空题'
  difficulty: '入门' | '初级' | '中级' | '高级' | '专家'
  domain: string
  options?: string[]
  answer: string
  explanation?: string
  timeEstimate: number
  tags?: string[]
}

// 单次答题记录
export interface QuizAnswer {
  questionId: string
  userAnswer: string
  correct: boolean
  date: string  // YYYY-MM-DD
  timestamp: number
  timeSpent?: number  // 秒
}

// 每日刷题计划
export interface DailyPlan {
  date: string  // YYYY-MM-DD
  questionIds: string[]
  stack: string  // 当天主推 stack
  completed: number
}

// 答题进度（按 stack 统计）
export interface StackProgress {
  stack: string
  total: number
  answered: number
  correct: number
  lastUpdated: string
}

// Store 接口
interface QuizState {
  questions: QuizQuestion[]
  answers: QuizAnswer[]
  dailyPlans: DailyPlan[]
  stackProgress: Record<string, StackProgress>

  // 当次测验状态
  currentSession: {
    questionIds: string[]
    currentIndex: number
    answers: Record<number, string>
    isAnswered: boolean
    startTime: number
  } | null

  // 方法
  startSession: (questionIds: string[]) => void
  submitAnswer: (answer: string, timeSpent?: number) => void
  nextQuestion: () => void
  prevQuestion: () => void
  endSession: () => void
  resetSession: () => void

  // 刷题计划
  generateDailyPlan: (date?: string) => DailyPlan
  getTodayPlan: () => DailyPlan | null
  updatePlanProgress: (questionId: string) => void

  // 错题本
  getWrongBook: () => QuizAnswer[]
  isWrongAnswer: (questionId: string) => boolean

  // 进度
  getProgress: (stack?: string) => StackProgress
  getStats: () => { totalAnswered: number; totalCorrect: number; streak: number; accuracy: number }

  // 辅助
  getQuestionById: (id: string) => QuizQuestion | undefined
  searchQuestions: (query: string) => QuizQuestion[]
}

// 真实题库数据 - 从旧版 reading-radar 迁移
const initialQuestions: QuizQuestion[] = allQuizQuestions as QuizQuestion[]

const STACK_ORDER = ['c', 'cpp', 'db', 'ds', 'linux', 'py', 'vdb']

// 计算日期字符串 YYYY-MM-DD
function todayStr(): string {
  return new Date().toISOString().slice(0, 10)
}

// 计算连续学习天数
function calculateStreak(answers: QuizAnswer[]): number {
  if (answers.length === 0) return 0
  const dates = new Set(answers.map(a => a.date))
  const sorted = Array.from(dates).sort().reverse()
  let streak = 0
  const today = new Date()
  for (let i = 0; i < sorted.length; i++) {
    const d = new Date(sorted[i])
    const diff = Math.floor((today.getTime() - d.getTime()) / 86400000)
    if (diff === streak) streak++
    else break
  }
  return streak
}

export const useQuizStore = create<QuizState>()(
  persist(
    (set, get) => ({
      questions: initialQuestions,
      answers: [],
      dailyPlans: [],
      stackProgress: {},
      currentSession: null,

      startSession: (questionIds) => {
        set({
          currentSession: {
            questionIds,
            currentIndex: 0,
            answers: {},
            isAnswered: false,
            startTime: Date.now()
          }
        })
      },

      submitAnswer: (answer, timeSpent) => {
        const session = get().currentSession
        if (!session) return
        const q = get().questions.find(qq => qq.id === session.questionIds[session.currentIndex])
        if (!q) return

        // 判分
        const userAns = String(answer).trim()
        const correctAns = String(q.answer).trim()
        const correct: boolean = userAns === correctAns ||
          (Boolean(q.options) && userAns === correctAns.charAt(0))

        const newAnswer: QuizAnswer = {
          questionId: q.id,
          userAnswer: answer,
          correct,
          date: todayStr(),
          timestamp: Date.now(),
          timeSpent
        }

        const allAnswers = [...get().answers, newAnswer]

        // 更新 stack 进度
        const stack = (q.tags?.[0] || 'c') as string
        const existing = get().stackProgress[stack] || {
          stack,
          total: quizByStack[stack]?.length || 0,
          answered: 0,
          correct: 0,
          lastUpdated: todayStr()
        }
        const newStackProgress = {
          ...get().stackProgress,
          [stack]: {
            ...existing,
            answered: existing.answered + 1,
            correct: existing.correct + (correct ? 1 : 0),
            lastUpdated: todayStr()
          }
        }

        set({
          answers: allAnswers,
          stackProgress: newStackProgress,
          currentSession: {
            ...session,
            answers: { ...session.answers, [session.currentIndex]: answer },
            isAnswered: true
          }
        })

        // 触发计划进度更新
        get().updatePlanProgress(q.id)
      },

      nextQuestion: () => {
        const session = get().currentSession
        if (!session) return
        if (session.currentIndex < session.questionIds.length - 1) {
          set({
            currentSession: {
              ...session,
              currentIndex: session.currentIndex + 1,
              isAnswered: !!session.answers[session.currentIndex + 1]
            }
          })
        }
      },

      prevQuestion: () => {
        const session = get().currentSession
        if (!session) return
        if (session.currentIndex > 0) {
          set({
            currentSession: {
              ...session,
              currentIndex: session.currentIndex - 1,
              isAnswered: !!session.answers[session.currentIndex - 1]
            }
          })
        }
      },

      endSession: () => {
        set({ currentSession: null })
      },

      resetSession: () => {
        set({ currentSession: null })
      },

      // 每日刷题计划 - 按 stack 轮换，每天推 10 道
      generateDailyPlan: (date) => {
        const dateStr = date || todayStr()
        // 用日期算出 stack 索引
        const daysSinceEpoch = Math.floor(Date.now() / 86400000)
        const stackIndex = daysSinceEpoch % STACK_ORDER.length
        const stack = STACK_ORDER[stackIndex]

        const stackQuestions = quizByStack[stack] || []
        // 选 10 道 - 混合难度（4 简单 + 4 中等 + 2 困难）
        const basic = stackQuestions.filter(q => q.difficulty === '入门' || q.difficulty === '初级')
        const medium = stackQuestions.filter(q => q.difficulty === '中级')
        const hard = stackQuestions.filter(q => q.difficulty === '高级' || q.difficulty === '专家')

        const pickRandom = (arr: QuizQuestion[], n: number) => {
          const shuffled = [...arr].sort(() => Math.random() - 0.5)
          return shuffled.slice(0, n)
        }

        const selected = [
          ...pickRandom(basic, 4),
          ...pickRandom(medium, 4),
          ...pickRandom(hard, 2)
        ].slice(0, 10)

        // 优先选用户没答过的
        const answeredIds = new Set(get().answers.map(a => a.questionId))
        const unanswered = selected.filter(q => !answeredIds.has(q.id))
        const final = unanswered.length >= 5 ? unanswered : selected

        const plan: DailyPlan = {
          date: dateStr,
          questionIds: final.map(q => q.id),
          stack,
          completed: 0
        }

        set({ dailyPlans: [...get().dailyPlans.filter(p => p.date !== dateStr), plan] })
        return plan
      },

      getTodayPlan: () => {
        const date = todayStr()
        return get().dailyPlans.find(p => p.date === date) || null
      },

      updatePlanProgress: (questionId) => {
        const date = todayStr()
        const plans = get().dailyPlans
        const plan = plans.find(p => p.date === date)
        if (plan && plan.questionIds.includes(questionId)) {
          const updated = { ...plan, completed: plan.completed + 1 }
          set({ dailyPlans: plans.map(p => p.date === date ? updated : p) })
        }
      },

      // 错题本
      getWrongBook: () => {
        // 同一道题只保留最近一次错答
        const wrongMap = new Map<string, QuizAnswer>()
        for (const a of get().answers) {
          if (!a.correct) {
            const existing = wrongMap.get(a.questionId)
            if (!existing || a.timestamp > existing.timestamp) {
              wrongMap.set(a.questionId, a)
            }
          }
        }
        return Array.from(wrongMap.values()).sort((a, b) => b.timestamp - a.timestamp)
      },

      isWrongAnswer: (questionId) => {
        return get().answers.some(a => a.questionId === questionId && !a.correct)
      },

      // 进度
      getProgress: (stack) => {
        if (stack) {
          return get().stackProgress[stack] || {
            stack,
            total: quizByStack[stack]?.length || 0,
            answered: 0,
            correct: 0,
            lastUpdated: todayStr()
          }
        }
        // 总进度
        const all = Object.values(get().stackProgress)
        return {
          stack: 'all',
          total: quizStats.total,
          answered: all.reduce((s, p) => s + p.answered, 0),
          correct: all.reduce((s, p) => s + p.correct, 0),
          lastUpdated: todayStr()
        }
      },

      getStats: () => {
        const answers = get().answers
        const totalAnswered = answers.length
        const totalCorrect = answers.filter(a => a.correct).length
        const accuracy = totalAnswered > 0 ? totalCorrect / totalAnswered : 0
        const streak = calculateStreak(answers)
        return { totalAnswered, totalCorrect, streak, accuracy }
      },

      // 辅助
      getQuestionById: (id) => {
        return get().questions.find(q => q.id === id)
      },

      searchQuestions: (query) => {
        const q = query.toLowerCase().trim()
        if (!q) return []
        return get().questions.filter(item =>
          item.title.toLowerCase().includes(q) ||
          item.description?.toLowerCase().includes(q) ||
          item.tags?.some(t => t.toLowerCase().includes(q))
        ).slice(0, 50)
      }
    }),
    {
      name: 'quiz-storage',
      version: 2  // 升级版本避免旧数据冲突
    }
  )
)
