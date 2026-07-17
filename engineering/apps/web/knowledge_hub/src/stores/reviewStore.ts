import { create } from 'zustand'
import { persist } from 'zustand/middleware'

// SM-2 复习记录接口
export interface ReviewRecord {
  id: string
  question: string
  answer: string
  domain: string
  easiness: number  // 难度因子 (默认 2.5)
  interval: number   // 间隔天数
  repetitions: number // 重复次数
  nextReviewDate: string
  lastReviewDate: string
}

// Store 接口
interface ReviewState {
  records: ReviewRecord[]
  dueReviews: ReviewRecord[]
  currentReview: ReviewRecord | null
  currentIndex: number
  loading: boolean

  loadDueReviews: () => void
  rateReview: (quality: number) => void  // 0-5 质量评分
  nextReview: () => void
  calculateNextReview: (record: ReviewRecord, quality: number) => ReviewRecord
  addRecord: (record: Omit<ReviewRecord, 'easiness' | 'interval' | 'repetitions' | 'nextReviewDate' | 'lastReviewDate'>) => void
}

// 初始示例数据
const initialRecords: ReviewRecord[] = [
  {
    id: 'review-1',
    question: '什么是时间复杂度？',
    answer: '时间复杂度是算法执行时间与输入规模之间关系的度量，用大 O 表示法描述。常见的有 O(1)、O(log n)、O(n)、O(n log n)、O(n²) 等。',
    domain: '算法',
    easiness: 2.5,
    interval: 1,
    repetitions: 0,
    nextReviewDate: new Date().toISOString(),
    lastReviewDate: ''
  },
  {
    id: 'review-2',
    question: 'C++ 中智能指针有哪些？',
    answer: 'C++ 中的智能指针包括：\n1. unique_ptr - 独占所有权\n2. shared_ptr - 共享所有权，引用计数\n3. weak_ptr - 弱引用，避免循环引用',
    domain: 'C/C++',
    easiness: 2.5,
    interval: 1,
    repetitions: 0,
    nextReviewDate: new Date().toISOString(),
    lastReviewDate: ''
  },
  {
    id: 'review-3',
    question: 'TCP 和 UDP 的区别是什么？',
    answer: 'TCP 是面向连接、可靠的传输协议，提供拥塞控制和流量控制；UDP 是无连接、不可靠的协议，但传输效率更高。TCP 适用于需要可靠传输的场景，UDP 适用于实时性要求高的场景。',
    domain: '计算机网络',
    easiness: 2.5,
    interval: 1,
    repetitions: 0,
    nextReviewDate: new Date().toISOString(),
    lastReviewDate: ''
  }
]

export const useReviewStore = create<ReviewState>()(
  persist(
    (set, get) => ({
      records: initialRecords,
      dueReviews: [],
      currentReview: null,
      currentIndex: 0,
      loading: false,

      // 加载待复习的记录
      loadDueReviews: () => {
        set({ loading: true })
        const now = new Date()
        const due = get().records.filter(record => {
          const nextReview = new Date(record.nextReviewDate)
          return nextReview <= now
        })
        set({
          dueReviews: due,
          currentReview: due.length > 0 ? due[0] : null,
          currentIndex: 0,
          loading: false
        })
      },

      // SM-2 算法核心：计算下次复习时间
      calculateNextReview: (record, quality) => {
        let { easiness, interval, repetitions } = record

        // 更新难度因子
        easiness = Math.max(1.3, easiness + (0.1 - (5 - quality) * (0.08 + (5 - quality) * 0.02)))

        if (quality < 3) {
          // 质量低于 3，重置间隔
          repetitions = 0
          interval = 1
        } else {
          // 质量 >= 3，增加间隔
          if (repetitions === 0) {
            interval = 1
          } else if (repetitions === 1) {
            interval = 6
          } else {
            interval = Math.round(interval * easiness)
          }
          repetitions += 1
        }

        // 计算下次复习日期
        const nextDate = new Date()
        nextDate.setDate(nextDate.getDate() + interval)

        return {
          ...record,
          easiness,
          interval,
          repetitions,
          nextReviewDate: nextDate.toISOString(),
          lastReviewDate: new Date().toISOString()
        }
      },

      // 对复习质量评分
      rateReview: (quality) => {
        const { currentReview, dueReviews, currentIndex, records } = get()
        if (!currentReview) return

        // 使用 SM-2 算法计算下次复习时间
        const updatedRecord = get().calculateNextReview(currentReview, quality)

        // 更新 records
        const updatedRecords = records.map(r =>
          r.id === updatedRecord.id ? updatedRecord : r
        )

        // 更新待复习列表
        const updatedDue = dueReviews.map(r =>
          r.id === updatedRecord.id ? updatedRecord : r
        ).filter(r => new Date(r.nextReviewDate) <= new Date())

        // 移动到下一个
        const nextIndex = currentIndex + 1
        const nextReview = nextIndex < updatedDue.length ? updatedDue[nextIndex] : null

        set({
          records: updatedRecords,
          dueReviews: updatedDue,
          currentReview: nextReview,
          currentIndex: nextIndex
        })
      },

      // 下一轮复习
      nextReview: () => {
        get().loadDueReviews()
      },

      // 添加新的复习记录
      addRecord: (record) => {
        const newRecord: ReviewRecord = {
          ...record,
          easiness: 2.5,
          interval: 1,
          repetitions: 0,
          nextReviewDate: new Date().toISOString(),
          lastReviewDate: ''
        }
        set(state => ({
          records: [...state.records, newRecord],
          dueReviews: [...state.dueReviews, newRecord]
        }))
      }
    }),
    {
      name: 'review-storage'
    }
  )
)
