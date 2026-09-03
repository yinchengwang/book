/**
 * 面试题库浏览状态
 * 已浏览 / 收藏
 */
import { create } from 'zustand'
import { persist } from 'zustand/middleware'

interface InterviewBankState {
  viewed: string[]       // 已浏览过的题目 ID
  bookmarks: string[]    // 收藏的题目 ID

  markViewed: (id: string) => void
  toggleBookmark: (id: string) => void
  isViewed: (id: string) => boolean
  isBookmarked: (id: string) => boolean
  clearViewed: () => void
}

export const useInterviewBankStore = create<InterviewBankState>()(
  persist(
    (set, get) => ({
      viewed: [],
      bookmarks: [],

      markViewed: (id) => {
        const { viewed } = get()
        if (viewed.includes(id)) return
        set({ viewed: [...viewed, id] })
      },

      toggleBookmark: (id) => {
        const { bookmarks } = get()
        if (bookmarks.includes(id)) {
          set({ bookmarks: bookmarks.filter(b => b !== id) })
        } else {
          set({ bookmarks: [...bookmarks, id] })
        }
      },

      isViewed: (id) => get().viewed.includes(id),
      isBookmarked: (id) => get().bookmarks.includes(id),

      clearViewed: () => set({ viewed: [] })
    }),
    { name: 'rr2-interview-bank' }
  )
)
