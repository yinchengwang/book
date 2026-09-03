/**
 * 全局通知 Store
 *
 * 动态从各业务 store 派生通知项：
 * - 复习提醒：reviewStore.dueReviews
 * - 面试动态：interviewStore
 * - 连续学习：fiveYearPlanStore.getStreak()
 * - 错题提醒：quizStore
 *
 * 通知点击可跳转到对应页面（link 字段）
 */
import { create } from 'zustand'

// 通知项接口
export interface Notification {
  id: string
  type: 'review' | 'interview' | 'streak' | 'wrongbook' | 'system'
  title: string
  desc?: string
  time: string           // 人类可读的时间（"2 小时前"）
  timestamp: number      // 用于排序和"X 分钟前"计算
  link?: string          // 点击跳转路径
  icon: string           // emoji
  iconBg: string         // 图标背景色（rgba）
  read: boolean
}

interface NotificationState {
  items: Notification[]
  unreadCount: number
  setItems: (items: Notification[]) => void
  markRead: (id: string) => void
  markAllRead: () => void
}

/**
 * 把绝对时间转为"X 分钟前 / X 小时前 / X 天前"
 */
export function timeAgo(timestamp: number): string {
  const diff = Date.now() - timestamp
  const minutes = Math.floor(diff / 60000)
  if (minutes < 1) return '刚刚'
  if (minutes < 60) return `${minutes} 分钟前`
  const hours = Math.floor(minutes / 60)
  if (hours < 24) return `${hours} 小时前`
  const days = Math.floor(hours / 24)
  if (days === 1) return '昨天'
  if (days < 7) return `${days} 天前`
  return new Date(timestamp).toLocaleDateString('zh-CN')
}

export const useNotificationStore = create<NotificationState>((set, get) => ({
  items: [],
  unreadCount: 0,

  setItems: (items) => {
    const unreadCount = items.filter(n => !n.read).length
    set({ items, unreadCount })
  },

  markRead: (id) => {
    const items = get().items.map(n => n.id === id ? { ...n, read: true } : n)
    const unreadCount = items.filter(n => !n.read).length
    set({ items, unreadCount })
  },

  markAllRead: () => {
    const items = get().items.map(n => ({ ...n, read: true }))
    set({ items, unreadCount: 0 })
  }
}))