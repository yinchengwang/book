import { create } from 'zustand'
import { persist } from 'zustand/middleware'

// 项目接口
export interface Project {
  id: string
  name: string
  description?: string
  status: '规划中' | '进行中' | '已完成' | '暂停' | '废弃'
  priority: '高' | '中' | '低'
  tags: string[]
  knowledgeLinks: string[]
  createdAt: string
  updatedAt: string
}

// Store 接口
interface ProjectState {
  projects: Project[]
  addProject: (project: Project) => void
  updateProject: (id: string, updates: Partial<Project>) => void
  deleteProject: (id: string) => void
}

// 初始示例数据
const initialProjects: Project[] = [
  {
    id: 'project-1',
    name: '手写 STL 容器库',
    description: '深入理解 STL 底层实现，手写 vector、list、map 等容器',
    status: '进行中',
    priority: '高',
    tags: ['C++', '数据结构', 'STL'],
    knowledgeLinks: [],
    createdAt: '2024-01-10',
    updatedAt: '2024-02-15'
  },
  {
    id: 'project-2',
    name: '分布式缓存系统',
    description: '实现一个简单的分布式缓存，支持一致性哈希',
    status: '规划中',
    priority: '中',
    tags: ['系统设计', 'Redis', '分布式'],
    knowledgeLinks: [],
    createdAt: '2024-02-01',
    updatedAt: '2024-02-01'
  },
  {
    id: 'project-3',
    name: 'LeetCode 刷题笔记',
    description: '整理 LeetCode 高频题型和解题思路',
    status: '已完成',
    priority: '中',
    tags: ['算法', '面试'],
    knowledgeLinks: [],
    createdAt: '2023-12-01',
    updatedAt: '2024-01-20'
  }
]

export const useProjectStore = create<ProjectState>()(
  persist(
    (set) => ({
      projects: initialProjects,

      // 添加项目
      addProject: (project) => set((state) => ({
        projects: [project, ...state.projects]
      })),

      // 更新项目
      updateProject: (id, updates) => set((state) => ({
        projects: state.projects.map(p =>
          p.id === id
            ? { ...p, ...updates, updatedAt: new Date().toISOString() }
            : p
        )
      })),

      // 删除项目
      deleteProject: (id) => set((state) => ({
        projects: state.projects.filter(p => p.id !== id)
      }))
    }),
    {
      name: 'project-storage'
    }
  )
)
