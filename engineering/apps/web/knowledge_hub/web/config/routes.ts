/**
 * 路由配置 + 导航菜单
 *
 * 路径 = 真实路由（不带前缀 /pages）
 * element = 懒加载的页面组件
 * icon = 侧边栏图标
 * group = 侧边栏分组
 * badgeGetter = 动态计算 badge 数字（函数，可在 Sidebar 中调用 store）
 */

import { lazy } from 'react'

export interface NavItem {
  path: string
  title: string
  icon: string
  group: string
  badge?: number | string
  badgeGetter?: () => number | string | null  // 动态 badge
}

export const NAV_ITEMS: NavItem[] = [
  // 主导航
  { path: '/', title: '仪表盘', icon: '🏠', group: '主导航' },

  // 练习 - 动态 badge
  {
    path: '/quiz',
    title: '题库练习',
    icon: '📝',
    group: '练习',
    badgeGetter: () => {
      // 总题数（动态从 quizByStack 累加）
      try {
        // 动态导入避免循环依赖
        const { quizStats } = require('@/data/quiz-index')
        return quizStats.total >= 1000 ? `${Math.floor(quizStats.total / 1000)}k+` : quizStats.total
      } catch { return null }
    }
  },
  {
    path: '/review',
    title: '复习计划',
    icon: '📖',
    group: '练习',
    badgeGetter: () => {
      try {
        const { useReviewStore } = require('@/stores/reviewStore')
        return useReviewStore.getState().dueReviews.length || null
      } catch { return null }
    }
  },

  // 面试
  {
    path: '/interview_tracker',
    title: '面试追踪',
    icon: '💼',
    group: '面试',
    badgeGetter: () => {
      try {
        const { useInterviewStore } = require('@/stores/interviewStore')
        const companies = useInterviewStore.getState().companies || []
        return companies.filter((c: any) =>
          ['hr', 'tech-1', 'tech-2', 'tech-3'].includes(c.stage)).length || null
      } catch { return null }
    }
  },
  { path: '/mock_interview', title: '模拟面试', icon: '🎤', group: '面试', badge: 'AI' },
  {
    path: '/interview_bank',
    title: '面试题库',
    icon: '📚',
    group: '面试',
    badgeGetter: () => {
      try {
        const { algoBank, interviewBank } = require('@/data/interview-bank')
        const total = (algoBank?.length || 0) + (interviewBank?.length || 0)
        return total >= 1000 ? `${Math.floor(total / 1000)}k+` : total
      } catch { return null }
    }
  },

  // 知识
  { path: '/knowledge_graph', title: '知识图谱', icon: '🔗', group: '知识' },
  { path: '/learning_path', title: '学习路径', icon: '🛤️', group: '知识' },
  { path: '/learn_deep', title: '图解系列', icon: '📖', group: '知识' },
  { path: '/radar', title: '技术雷达', icon: '🎯', group: '知识' },
  {
    path: '/excerpt',
    title: '摘抄管理',
    icon: '📚',
    group: '知识',
    badgeGetter: () => {
      try {
        const { useExcerptStore } = require('@/stores/excerptStore')
        return useExcerptStore.getState().excerpts.length || null
      } catch { return null }
    }
  },
  { path: '/gap_analysis', title: '差距分析', icon: '📊', group: '知识' },

  // 项目
  { path: '/project_roadmap', title: '项目路线', icon: '📁', group: '项目' },

  // 计划
  {
    path: '/five_year_plan',
    title: '五年计划',
    icon: '🏗️',
    group: '计划',
    badgeGetter: () => {
      try {
        const { useFiveYearPlanStore } = require('@/stores/fiveYearPlanStore')
        const streak = useFiveYearPlanStore.getState().getStreak()
        return streak > 0 ? `🔥${streak}` : null
      } catch { return null }
    }
  },

  // 系统
  { path: '/settings', title: '设置', icon: '⚙️', group: '系统' }
]

// 按 group 分组
export const NAV_GROUPS = NAV_ITEMS.reduce<Record<string, NavItem[]>>((acc, item) => {
  if (!acc[item.group]) acc[item.group] = []
  acc[item.group].push(item)
  return acc
}, {})

// 路由懒加载 - 每个页面单独 chunk
export const routeComponents: Record<string, React.LazyExoticComponent<React.ComponentType<any>>> = {
  '/': lazy(() => import('@/pages/index/index')),
  '/quiz': lazy(() => import('@/pages/quiz/Quiz')),
  '/review': lazy(() => import('@/pages/review/Review')),
  '/interview_tracker': lazy(() => import('@/pages/interview_tracker/interview_tracker')),
  '/mock_interview': lazy(() => import('@/pages/mock_interview/mock_interview')),
  '/interview_bank': lazy(() => import('@/pages/interview_bank/interview_bank')),
  '/knowledge_graph': lazy(() => import('@/pages/knowledge_graph/knowledge_graph')),
  '/learning_path': lazy(() => import('@/pages/learning_path/learning_path')),
  '/excerpt': lazy(() => import('@/pages/excerpt/Excerpt')),
  '/gap_analysis': lazy(() => import('@/pages/gap_analysis/gap_analysis')),
  '/project_roadmap': lazy(() => import('@/pages/project_roadmap/project_roadmap')),
  '/settings': lazy(() => import('@/pages/settings/Settings')),
  '/learn_deep': lazy(() => import('@/pages/learn_deep/LearnDeepCategories')),
  '/learn_deep/:category': lazy(() => import('@/pages/learn_deep/LearnDeepDetail')),
  '/radar': lazy(() => import('@/pages/radar/Radar')),
  '/five_year_plan': lazy(() => import('@/pages/five_year_plan/five_year_plan')),
  '/not-found': lazy(() => import('@/pages/not-found/NotFound'))
}