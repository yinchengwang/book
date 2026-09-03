/**
 * 路由配置文件
 * 统一管理所有页面路径
 */

// 页面路径常量
export const ROUTES = {
  // TabBar 页面（使用 switchTab）
  INDEX: '/pages/index',
  REVIEW: '/pages/review/Review',
  TRACKER: '/pages/interview_tracker/interview_tracker',
  LEARNING_PATH: '/pages/learning_path/learning_path',
  SETTINGS: '/pages/settings/Settings',

  // 普通页面（使用 navigateTo）
  KNOWLEDGE_GRAPH: '/pages/knowledge_graph/knowledge_graph',
  GAP_ANALYSIS: '/pages/gap_analysis/gap_analysis',
  EXCERPT: '/pages/excerpt/Excerpt',
  MOCK_INTERVIEW: '/pages/mock_interview/mock_interview',
  PROJECT_ROADMAP: '/pages/project_roadmap/project_roadmap',

  // 特殊页面
  NOT_FOUND: '/pages/not-found/NotFound',
} as const

// 路由名称
export const ROUTE_NAMES = {
  INDEX: 'index',
  REVIEW: 'review',
  TRACKER: 'tracker',
  LEARNING_PATH: 'learningPath',
  SETTINGS: 'settings',
  KNOWLEDGE_GRAPH: 'knowledgeGraph',
  GAP_ANALYSIS: 'gapAnalysis',
  EXCERPT: 'excerpt',
  MOCK_INTERVIEW: 'mockInterview',
  PROJECT_ROADMAP: 'projectRoadmap',
  NOT_FOUND: 'notFound',
} as const

// TabBar 页面列表
export const TAB_BAR_ROUTES = [
  ROUTES.INDEX,
  ROUTES.REVIEW,
  ROUTES.TRACKER,
  ROUTES.LEARNING_PATH,
  ROUTES.SETTINGS,
] as const

// 判断是否为 TabBar 页面
export const isTabBarPage = (path: string): boolean => {
  return TAB_BAR_ROUTES.includes(path as typeof TAB_BAR_ROUTES[number])
}

// TabBar 页面索引
export const getTabBarIndex = (path: string): number => {
  return TAB_BAR_ROUTES.indexOf(path as typeof TAB_BAR_ROUTES[number])
}

// 路由元信息
export interface RouteMeta {
  title: string
  isTabBar?: boolean
  requiresAuth?: boolean
}

export const ROUTE_META: Record<string, RouteMeta> = {
  [ROUTES.INDEX]: { title: '首页', isTabBar: true },
  [ROUTES.REVIEW]: { title: '复习', isTabBar: true },
  [ROUTES.TRACKER]: { title: '面试', isTabBar: true },
  [ROUTES.LEARNING_PATH]: { title: '学习路径', isTabBar: true },
  [ROUTES.SETTINGS]: { title: '设置', isTabBar: true },
  [ROUTES.KNOWLEDGE_GRAPH]: { title: '知识图谱' },
  [ROUTES.GAP_ANALYSIS]: { title: '差距分析' },
  [ROUTES.EXCERPT]: { title: '摘抄管理' },
  [ROUTES.MOCK_INTERVIEW]: { title: '模拟面试' },
  [ROUTES.PROJECT_ROADMAP]: { title: '项目路线' },
  [ROUTES.NOT_FOUND]: { title: '页面不存在' },
}

export default ROUTES
