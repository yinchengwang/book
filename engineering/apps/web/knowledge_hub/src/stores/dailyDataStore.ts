/**
 * 每日数据存储
 * 存储成长趋势和热力图所需的数据
 */
import { create } from 'zustand'
import { persist } from 'zustand/middleware'

// 快照数据
export interface Snapshot {
  date: string           // 'YYYY-MM-DD'
  scores: Record<string, number> // domain -> score
  totalScore: number     // 综合掌握度
}

// 每日日志
export interface DailyLog {
  date: string
  studied: string[]      // 题目 ID 列表
  correct: string[]
  wrong: string[]
  reviewCount: number    // 复习次数
  studyMinutes: number   // 学习时长（分钟）
}

// 每日数据 Store
interface DailyDataState {
  // 快照历史
  snapshots: Snapshot[]
  // 每日日志
  dailyLogs: Record<string, DailyLog>

  // 添加快照
  addSnapshot: (snapshot: Snapshot) => void
  // 获取最近 N 天的快照
  getRecentSnapshots: (days: number) => Snapshot[]
  // 获取某日日志
  getDailyLog: (date: string) => DailyLog | undefined
  // 添加每日日志
  addDailyLog: (log: DailyLog) => void
  // 更新学习活动
  recordStudy: (itemId: string, correct: boolean) => void
  // 记录复习
  recordReview: (count?: number) => void
  // 记录学习时长
  recordStudyTime: (minutes: number) => void
  // 获取热力图数据（最近一年）
  getHeatmapData: () => Record<string, number>
}

export const useDailyDataStore = create<DailyDataState>()(
  persist(
    (set, get) => ({
      snapshots: [],
      dailyLogs: {},

      addSnapshot: (snapshot) => {
        set(state => {
          // 检查是否已存在该日期的快照
          const existing = state.snapshots.findIndex(s => s.date === snapshot.date)
          if (existing >= 0) {
            // 更新
            const newSnapshots = [...state.snapshots]
            newSnapshots[existing] = snapshot
            return { snapshots: newSnapshots }
          } else {
            // 添加
            return { snapshots: [...state.snapshots, snapshot] }
          }
        })
      },

      getRecentSnapshots: (days) => {
        const { snapshots } = get()
        const cutoffDate = new Date()
        cutoffDate.setDate(cutoffDate.getDate() - days)

        return snapshots
          .filter(s => new Date(s.date) >= cutoffDate)
          .sort((a, b) => a.date.localeCompare(b.date))
      },

      getDailyLog: (date) => {
        return get().dailyLogs[date]
      },

      addDailyLog: (log) => {
        set(state => ({
          dailyLogs: {
            ...state.dailyLogs,
            [log.date]: log
          }
        }))
      },

      recordStudy: (itemId, correct) => {
        const today = new Date().toISOString().slice(0, 10)
        set(state => {
          const existing = state.dailyLogs[today] || {
            date: today,
            studied: [],
            correct: [],
            wrong: [],
            reviewCount: 0,
            studyMinutes: 0
          }

          const newLog = {
            ...existing,
            studied: [...new Set([...existing.studied, itemId])],
            correct: correct
              ? [...new Set([...existing.correct, itemId])]
              : existing.correct,
            wrong: !correct
              ? [...new Set([...existing.wrong, itemId])]
              : existing.wrong,
          }

          return {
            dailyLogs: {
              ...state.dailyLogs,
              [today]: newLog
            }
          }
        })
      },

      recordReview: (count = 1) => {
        const today = new Date().toISOString().slice(0, 10)
        set(state => {
          const existing = state.dailyLogs[today] || {
            date: today,
            studied: [],
            correct: [],
            wrong: [],
            reviewCount: 0,
            studyMinutes: 0
          }

          return {
            dailyLogs: {
              ...state.dailyLogs,
              [today]: {
                ...existing,
                reviewCount: existing.reviewCount + count
              }
            }
          }
        })
      },

      recordStudyTime: (minutes) => {
        const today = new Date().toISOString().slice(0, 10)
        set(state => {
          const existing = state.dailyLogs[today] || {
            date: today,
            studied: [],
            correct: [],
            wrong: [],
            reviewCount: 0,
            studyMinutes: 0
          }

          return {
            dailyLogs: {
              ...state.dailyLogs,
              [today]: {
                ...existing,
                studyMinutes: existing.studyMinutes + minutes
              }
            }
          }
        })
      },

      getHeatmapData: () => {
        const { dailyLogs } = get()
        const result: Record<string, number> = {}

        // 获取最近一年的数据
        const oneYearAgo = new Date()
        oneYearAgo.setFullYear(oneYearAgo.getFullYear() - 1)

        for (const [date, log] of Object.entries(dailyLogs)) {
          if (new Date(date) >= oneYearAgo) {
            // 计算当日学习强度（题目数 + 复习数）
            result[date] = (log.studied?.length || 0) + (log.reviewCount || 0)
          }
        }

        return result
      },
    }),
    {
      name: 'daily-data-storage',
    }
  )
)

// 创建每日快照（定时任务）
export const createDailySnapshot = (scores: Record<string, number>) => {
  const today = new Date().toISOString().slice(0, 10)
  const totalScore = Object.values(scores).reduce((a, b) => a + b, 0) / Math.max(Object.values(scores).length, 1)

  useDailyDataStore.getState().addSnapshot({
    date: today,
    scores,
    totalScore,
  })
}
