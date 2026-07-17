/**
 * 五年计划打卡数据 Store
 * 按 year/month/day 组织每日打卡数据
 *
 * 数据结构：
 *   data: { '2026': { '06': { '08': { checks: {...}, note: '...', hasLearning: true } } } }
 */
import { create } from 'zustand'
import { persist } from 'zustand/middleware'
import { getAllItems, YEAR_CONFIG } from '@/data/five_year_plan_config'

// 单日数据
export interface DayData {
  checks: Record<string, boolean>  // habitId -> checked
  note: string
  hasLearning?: boolean
}

interface FiveYearPlanState {
  // 嵌套数据：year -> month -> day -> data
  data: Record<string, Record<string, Record<string, DayData>>>

  // 读：取某日数据
  getDay: (year: number, month: number, day: number) => DayData
  // 写：保存某日数据
  setDay: (year: number, month: number, day: number, data: DayData) => void
  // 删除某日数据
  deleteDay: (year: number, month: number, day: number) => void
  // 快速切换某日某项
  toggleCheck: (year: number, month: number, day: number, habitId: string) => void
  // 统计
  countDayChecks: (year: number, month: number, day: number) => { done: number; total: number }
  // 统计连续打卡天数
  getStreak: () => number
  // 统计累计打卡天数
  getTotalDays: () => number
  // 当月完成率
  getMonthPct: (year: number, month: number) => number
  // 切换学习标记
  toggleLearning: (year: number, month: number, day: number) => void
}

const pad = (n: number) => String(n).padStart(2, '0')

const emptyDay = (): DayData => ({ checks: {}, note: '' })

export const useFiveYearPlanStore = create<FiveYearPlanState>()(
  persist(
    (set, get) => ({
      data: {},

      getDay: (year, month, day) => {
        const y = String(year)
        const m = pad(month)
        const d = pad(day)
        return get().data[y]?.[m]?.[d] || emptyDay()
      },

      setDay: (year, month, day, data) => {
        const y = String(year)
        const m = pad(month)
        const d = pad(day)
        set(state => {
          const next = { ...state.data }
          if (!next[y]) next[y] = {}
          if (!next[y][m]) next[y][m] = {}
          next[y][m] = { ...next[y][m], [d]: data }
          return { data: next }
        })
      },

      deleteDay: (year, month, day) => {
        const y = String(year)
        const m = pad(month)
        const d = pad(day)
        set(state => {
          const yearData = state.data[y]
          if (!yearData?.[m]?.[d]) return state
          const nextMonth = { ...yearData[m] }
          delete nextMonth[d]
          const nextYear = { ...yearData, [m]: nextMonth }
          if (Object.keys(nextMonth).length === 0) delete nextYear[m]
          return { data: { ...state.data, [y]: nextYear } }
        })
      },

      toggleCheck: (year, month, day, habitId) => {
        const dayData = get().getDay(year, month, day)
        const newChecks = { ...dayData.checks, [habitId]: !dayData.checks[habitId] }
        get().setDay(year, month, day, { ...dayData, checks: newChecks })
      },

      toggleLearning: (year, month, day) => {
        const dayData = get().getDay(year, month, day)
        get().setDay(year, month, day, { ...dayData, hasLearning: !dayData.hasLearning })
      },

      countDayChecks: (year, month, day) => {
        const dayData = get().getDay(year, month, day)
        const items = getAllItems(year)
        let done = 0
        for (const item of items) {
          if (dayData.checks[item.id]) done++
        }
        return { done, total: items.length }
      },

      getStreak: () => {
        const d = new Date()
        let streak = 0
        // 最多回溯 365 天防止死循环
        for (let i = 0; i < 365; i++) {
          const { done } = get().countDayChecks(d.getFullYear(), d.getMonth() + 1, d.getDate())
          if (done > 0) {
            streak++
            d.setDate(d.getDate() - 1)
          } else {
            break
          }
        }
        return streak
      },

      getTotalDays: () => {
        const { data } = get()
        let total = 0
        for (const months of Object.values(data)) {
          for (const days of Object.values(months)) {
            for (const dayData of Object.values(days)) {
              if (Object.values(dayData.checks).some(v => v)) total++
            }
          }
        }
        return total
      },

      getMonthPct: (year, month) => {
        const m = pad(month)
        const monthData = get().data[String(year)]?.[m] || {}
        let total = 0, done = 0
        for (const [d, dayData] of Object.entries(monthData)) {
          const day = parseInt(d, 10)
          const c = get().countDayChecks(year, month, day)
          total += c.total
          done += c.done
        }
        return total > 0 ? Math.round(done / total * 100) : 0
      }
    }),
    { name: 'rr2-five-year-plan' }
  )
)
