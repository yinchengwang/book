/**
 * DataService 数据服务层
 * 封装 Zustand store 的 CRUD 操作，提供统一的数据访问接口
 */
import { useKnowledgeStore } from '@/stores/knowledgeStore'
import { useReviewStore } from '@/stores/reviewStore'
import { useInterviewStore } from '@/stores/interviewStore'
import { useLearningPathStore } from '@/stores/learningPathStore'
import { useExcerptStore } from '@/stores/excerptStore'
import { useProjectStore } from '@/stores/projectStore'

// 重新导出 stores
export { useKnowledgeStore, useReviewStore, useInterviewStore, useLearningPathStore, useExcerptStore, useProjectStore } from '@/stores'

// ============= 知识库服务 =============

export const KnowledgeService = {
  // 获取所有知识点
  getAll: () => {
    const store = useKnowledgeStore.getState()
    return store.items
  },

  // 按分类获取
  getByCategory: (category: string) => {
    const store = useKnowledgeStore.getState()
    return store.items.filter(item => item.category === category)
  },

  // 获取差距分析
  getGaps: () => {
    const store = useKnowledgeStore.getState()
    return store.gaps
  },

  // 更新掌握度
  updateMastery: (id: string, mastery: number) => {
    const store = useKnowledgeStore.getState()
    store.updateItem?.(id, { mastery })
  },
}

// ============= 复习服务 =============

export const ReviewService = {
  // 获取待复习项目
  getPendingReviews: () => {
    const store = useReviewStore.getState()
    return store.getReviewQueue?.() || []
  },

  // 获取复习统计
  getStats: () => {
    const store = useReviewStore.getState()
    return {
      total: store.totalReviews || 0,
      mastered: store.masteredCount || 0,
      learning: store.learningCount || 0,
      weak: store.weakCount || 0,
    }
  },

  // 执行复习（SM-2 算法）
  review: (itemId: string, rating: number) => {
    const store = useReviewStore.getState()
    store.reviewItem?.(itemId, rating)
  },

  // 获取复习历史
  getHistory: () => {
    const store = useReviewStore.getState()
    return store.history || []
  },
}

// ============= 面试追踪服务 =============

export const InterviewService = {
  // 获取所有面试记录
  getAll: () => {
    const store = useInterviewStore.getState()
    return store.companies || []
  },

  // 按状态获取
  getByStatus: (status: string) => {
    const store = useInterviewStore.getState()
    return store.companies?.filter(c => c.status === status) || []
  },

  // 创建面试记录
  create: (company: any) => {
    const store = useInterviewStore.getState()
    store.addCompany?.(company)
  },

  // 更新面试状态
  updateStatus: (id: string, status: string) => {
    const store = useInterviewStore.getState()
    store.updateCompany?.(id, { status })
  },

  // 删除面试记录
  delete: (id: string) => {
    const store = useInterviewStore.getState()
    store.deleteCompany?.(id)
  },
}

// ============= 学习路径服务 =============

export const LearningPathService = {
  // 获取所有学习路径
  getAll: () => {
    const store = useLearningPathStore.getState()
    return store.paths || []
  },

  // 获取路径进度
  getProgress: (pathId: string) => {
    const store = useLearningPathStore.getState()
    const path = store.paths?.find(p => p.id === pathId)
    if (!path) return 0
    const completed = path.steps?.filter(s => s.status === 'completed').length || 0
    const total = path.steps?.length || 0
    return total > 0 ? (completed / total) * 100 : 0
  },

  // 更新步骤状态
  updateStepStatus: (pathId: string, stepId: string, status: string) => {
    const store = useLearningPathStore.getState()
    store.updateStep?.(pathId, stepId, status)
  },
}

// ============= 摘抄服务 =============

export const ExcerptService = {
  // 获取所有摘抄
  getAll: () => {
    const store = useExcerptStore.getState()
    return store.excerpts || []
  },

  // 按书籍获取
  getByBook: (bookId: string) => {
    const store = useExcerptStore.getState()
    return store.excerpts?.filter(e => e.bookId === bookId) || []
  },

  // 按标签获取
  getByTag: (tag: string) => {
    const store = useExcerptStore.getState()
    return store.excerpts?.filter(e => e.tags?.includes(tag)) || []
  },

  // 搜索
  search: (keyword: string) => {
    const store = useExcerptStore.getState()
    return store.excerpts?.filter(e =>
      e.content?.includes(keyword) ||
      e.note?.includes(keyword)
    ) || []
  },

  // 创建摘抄
  create: (excerpt: any) => {
    const store = useExcerptStore.getState()
    store.addExcerpt?.(excerpt)
  },

  // 更新摘抄
  update: (id: string, data: any) => {
    const store = useExcerptStore.getState()
    store.updateExcerpt?.(id, data)
  },

  // 删除摘抄
  delete: (id: string) => {
    const store = useExcerptStore.getState()
    store.deleteExcerpt?.(id)
  },
}

// ============= 项目服务 =============

export const ProjectService = {
  // 获取所有项目
  getAll: () => {
    const store = useProjectStore.getState()
    return store.projects || []
  },

  // 创建项目
  create: (project: any) => {
    const store = useProjectStore.getState()
    store.addProject?.(project)
  },

  // 更新项目
  update: (id: string, data: any) => {
    const store = useProjectStore.getState()
    store.updateProject?.(id, data)
  },

  // 删除项目
  delete: (id: string) => {
    const store = useProjectStore.getState()
    store.deleteProject?.(id)
  },
}

// ============= 统一导出 =============

export const DataService = {
  knowledge: KnowledgeService,
  review: ReviewService,
  interview: InterviewService,
  learningPath: LearningPathService,
  excerpt: ExcerptService,
  project: ProjectService,
}

export default DataService
