import { create } from 'zustand'
import { persist } from 'zustand/middleware'
import { initialExcerpts } from '@/data/excerpt/initialExcerpts'

// 阅读状态
export type ReadStatus = 'reading' | 'read' | 'mastered' | 'wishlist'

// 摘抄接口（增加 date / source / status 字段）
export interface Excerpt {
  id: string
  content: string
  book: string
  tags: string[]
  note?: string
  favorite: boolean
  createdAt: string
  // 新字段（迁移数据来源：老项目 project/reading-radar/data/excerpt/）
  date?: string         // 年份（2024 / 2025）
  source?: string       // 源文件名（不含 .md 后缀）
  status?: ReadStatus   // 阅读状态（来自 excerpt-meta.json）
}

// Store 接口
interface ExcerptState {
  excerpts: Excerpt[]
  filterBook: string
  filterTag: string
  filterYear: string    // 新增：按年份过滤
  searchQuery: string

  // Actions
  setFilterBook: (book: string) => void
  setFilterTag: (tag: string) => void
  setFilterYear: (year: string) => void
  setSearchQuery: (query: string) => void
  addExcerpt: (excerpt: Excerpt) => void
  deleteExcerpt: (id: string) => void
  toggleFavorite: (id: string) => void
  updateNote: (id: string, note: string) => void
  setStatus: (id: string, status: ReadStatus | undefined) => void
}

export const useExcerptStore = create<ExcerptState>()(
  persist(
    (set) => ({
      // 初始数据：4 条演示 + 老项目 145 条迁移数据
      excerpts: initialExcerpts,
      filterBook: '',
      filterTag: '',
      filterYear: '',
      searchQuery: '',

      setFilterBook: (filterBook) => set({ filterBook }),
      setFilterTag: (filterTag) => set({ filterTag }),
      setFilterYear: (filterYear) => set({ filterYear }),
      setSearchQuery: (searchQuery) => set({ searchQuery }),

      addExcerpt: (excerpt) => set((state) => ({
        excerpts: [excerpt, ...state.excerpts]
      })),

      deleteExcerpt: (id) => set((state) => ({
        excerpts: state.excerpts.filter(e => e.id !== id)
      })),

      toggleFavorite: (id) => set((state) => ({
        excerpts: state.excerpts.map(e =>
          e.id === id ? { ...e, favorite: !e.favorite } : e
        )
      })),

      updateNote: (id, note) => set((state) => ({
        excerpts: state.excerpts.map(e =>
          e.id === id ? { ...e, note } : e
        )
      })),

      setStatus: (id, status) => set((state) => ({
        excerpts: state.excerpts.map(e =>
          e.id === id ? { ...e, status } : e
        )
      }))
    }),
    {
      name: 'excerpt-storage',
      version: 2,
      migrate: (persistedState: any, version: number) => {
        // 版本迁移：老版本（v1）无 date / source / status 字段，保留兼容
        // 同时把 v1 老数据 tags (string) 规范化为 string[]，避免 Excerpt.tsx:62 的 ex.tags.includes 崩溃
        if (version < 2 && persistedState?.excerpts) {
          // 老版本的 excerpts 与新版本结构不同（没有 date/source/status），合并新数据
          const oldExcerpts = persistedState.excerpts as any[]
          // 兜底：tags 强制转数组
          const normalizedOld = oldExcerpts.map(e => ({
            ...e,
            tags: Array.isArray(e.tags) ? e.tags : (e.tags ? [String(e.tags)] : [])
          })) as Excerpt[]
          // 保留老用户数据（按 id 去重）
          const oldIds = new Set(normalizedOld.map(e => e.id))
          const newExcerpts = initialExcerpts.filter(e => !oldIds.has(e.id))
          return {
            ...persistedState,
            excerpts: [...normalizedOld, ...newExcerpts]
          }
        }
        return persistedState
      }
    }
  )
)

// 辅助 hook：派生所有书籍、标签、年份、状态
export const useExcerptDerivedData = () => {
  const excerpts = useExcerptStore(state => state.excerpts)

  // 兜底：任何残留的非数组 tags 都规范化为 []，避免 .flatMap/.includes 崩溃
  const safeExcerpts = excerpts.map(e => ({
    ...e,
    tags: Array.isArray(e.tags) ? e.tags : []
  }))

  const books = Array.from(new Set(safeExcerpts.map(e => e.book))).sort()
  const tags = Array.from(new Set(safeExcerpts.flatMap(e => e.tags))).sort()
  const years = Array.from(new Set(safeExcerpts.map(e => e.date).filter(Boolean) as string[])).sort()

  const stats = {
    total: safeExcerpts.length,
    favorites: safeExcerpts.filter(e => e.favorite).length,
    reading: safeExcerpts.filter(e => e.status === 'reading').length,
    mastered: safeExcerpts.filter(e => e.status === 'mastered').length,
    books: new Set(safeExcerpts.map(e => e.book)).size
  }

  return { books, tags, years, stats }
}