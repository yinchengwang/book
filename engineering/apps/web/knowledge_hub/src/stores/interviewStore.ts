import { create } from 'zustand'
import { persist } from 'zustand/middleware'

// 面试记录接口
export interface InterviewRecord {
  id: string
  date: string
  content: string
  type: '一面' | '二面' | '三面' | 'HR' | '笔试'
}

// 公司接口
export interface Company {
  id: string
  name: string
  position: string
  status: '投递' | '笔试' | '一面' | '二面' | '三面' | 'HR' | 'offer' | '拒绝'
  salary?: string
  notes?: string
  timeline: InterviewRecord[]
  createdAt: string
}

// Store 接口
interface InterviewState {
  companies: Company[]
  totalInterviews: number
  upcomingInterviews: Company[]
  addCompany: (company: Company) => void
  removeCompany: (id: string) => void
  updateStatus: (id: string, status: Company['status']) => void
  addInterviewRecord: (companyId: string, record: Omit<InterviewRecord, 'id'>) => void
}

// 初始示例数据
const initialCompanies: Company[] = [
  {
    id: 'company-1',
    name: '字节跳动',
    position: '后端开发工程师',
    status: '二面',
    salary: '35-50K',
    notes: '看重算法能力',
    timeline: [
      { id: 'rec-1', date: '2024-01-15', content: '投递简历', type: '一面' },
      { id: 'rec-2', date: '2024-01-20', content: '笔试通过', type: '笔试' },
      { id: 'rec-3', date: '2024-01-25', content: '一面：算法+系统设计', type: '一面' }
    ],
    createdAt: '2024-01-15'
  },
  {
    id: 'company-2',
    name: '阿里巴巴',
    position: 'C++开发',
    status: '笔试',
    salary: '30-45K',
    notes: '蚂蚁集团',
    timeline: [
      { id: 'rec-4', date: '2024-02-01', content: '内推投递', type: '一面' }
    ],
    createdAt: '2024-02-01'
  }
]

export const useInterviewStore = create<InterviewState>()(
  persist(
    (set, get) => ({
      companies: initialCompanies,
      totalInterviews: initialCompanies.length,
      upcomingInterviews: initialCompanies.filter(c =>
        c.status !== 'offer' && c.status !== '拒绝'
      ),

      // 添加公司
      addCompany: (company) => set((state) => ({
        companies: [...state.companies, company],
        totalInterviews: state.totalInterviews + 1,
        upcomingInterviews: [...state.upcomingInterviews, company]
      })),

      // 删除公司
      removeCompany: (id) => set((state) => ({
        companies: state.companies.filter(c => c.id !== id),
        totalInterviews: state.totalInterviews - 1,
        upcomingInterviews: state.upcomingInterviews.filter(c => c.id !== id)
      })),

      // 更新状态
      updateStatus: (id, status) => set((state) => {
        const updated = state.companies.map(c =>
          c.id === id ? { ...c, status } : c
        )
        return {
          companies: updated,
          upcomingInterviews: updated.filter(c =>
            c.status !== 'offer' && c.status !== '拒绝'
          )
        }
      }),

      // 添加面试记录
      addInterviewRecord: (companyId, record) => set((state) => ({
        companies: state.companies.map(c =>
          c.id === companyId
            ? { ...c, timeline: [...c.timeline, { ...record, id: `rec-${Date.now()}` }] }
            : c
        )
      }))
    }),
    {
      name: 'interview-storage'
    }
  )
)
