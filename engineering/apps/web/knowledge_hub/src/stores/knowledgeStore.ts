import { create } from 'zustand'
import { persist } from 'zustand/middleware'

// 知识领域接口
export interface KnowledgeNode {
  id: string
  name: string
  domain: string
  level: number
  parentId?: string
}

export interface KnowledgeLink {
  source: string
  target: string
  type: string
}

// 领域等级
export interface DomainLevel {
  level: number
  count: number
}

// Store 接口
interface KnowledgeState {
  nodes: KnowledgeNode[]
  links: KnowledgeLink[]
  knowledgeGraph: {
    nodes: KnowledgeNode[]
    links: KnowledgeLink[]
  }
  gaps: {
    algorithm?: DomainLevel
    system?: DomainLevel
    cpp?: DomainLevel
    database?: DomainLevel
    network?: DomainLevel
  }
  totalKnowledge: number
  addNode: (node: KnowledgeNode) => void
  removeNode: (id: string) => void
  updateNodeLevel: (id: string, level: number) => void
  loadGaps: () => void
}

// 初始化知识图谱数据
const initialNodes: KnowledgeNode[] = [
  { id: 'algo-1', name: '数组与链表', domain: '算法', level: 1 },
  { id: 'algo-2', name: '二叉树', domain: '算法', level: 2 },
  { id: 'algo-3', name: '动态规划', domain: '算法', level: 3 },
  { id: 'algo-4', name: '图论', domain: '算法', level: 4 },
  { id: 'sys-1', name: '进程与线程', domain: '系统设计', level: 2 },
  { id: 'sys-2', name: '内存管理', domain: '系统设计', level: 3 },
  { id: 'sys-3', name: '分布式系统', domain: '系统设计', level: 4 },
  { id: 'cpp-1', name: 'STL 容器', domain: 'C/C++', level: 2 },
  { id: 'cpp-2', name: '智能指针', domain: 'C/C++', level: 3 },
  { id: 'cpp-3', name: '模板元编程', domain: 'C/C++', level: 4 },
  { id: 'db-1', name: 'SQL 基础', domain: '数据库', level: 2 },
  { id: 'db-2', name: '索引原理', domain: '数据库', level: 3 },
  { id: 'net-1', name: 'TCP/IP', domain: '计算机网络', level: 2 },
  { id: 'net-2', name: 'HTTP/HTTPS', domain: '计算机网络', level: 3 }
]

const initialLinks: KnowledgeLink[] = [
  { source: 'algo-1', target: 'algo-2', type: 'prerequisite' },
  { source: 'algo-2', target: 'algo-3', type: 'prerequisite' },
  { source: 'algo-3', target: 'algo-4', type: 'prerequisite' },
  { source: 'sys-1', target: 'sys-2', type: 'prerequisite' },
  { source: 'sys-2', target: 'sys-3', type: 'prerequisite' },
  { source: 'cpp-1', target: 'cpp-2', type: 'prerequisite' },
  { source: 'cpp-2', target: 'cpp-3', type: 'prerequisite' },
  { source: 'db-1', target: 'db-2', type: 'prerequisite' },
  { source: 'net-1', target: 'net-2', type: 'prerequisite' }
]

export const useKnowledgeStore = create<KnowledgeState>()(
  persist(
    (set, get) => ({
      nodes: initialNodes,
      links: initialLinks,
      knowledgeGraph: {
        nodes: initialNodes,
        links: initialLinks
      },
      // 初始 gaps 为空：用户未开始学习时不应有基础数据
      // 数据由 loadGaps() 从 nodes 动态计算，或由答题正确率反推
      gaps: {},
      totalKnowledge: 14,

      addNode: (node) => set((state) => ({
        nodes: [...state.nodes, node],
        knowledgeGraph: {
          nodes: [...state.knowledgeGraph.nodes, node],
          links: state.knowledgeGraph.links
        },
        totalKnowledge: state.totalKnowledge + 1
      })),

      removeNode: (id) => set((state) => ({
        nodes: state.nodes.filter(n => n.id !== id),
        links: state.links.filter(l => l.source !== id && l.target !== id),
        knowledgeGraph: {
          nodes: state.knowledgeGraph.nodes.filter(n => n.id !== id),
          links: state.knowledgeGraph.links.filter(l => l.source !== id && l.target !== id)
        },
        totalKnowledge: state.totalKnowledge - 1
      })),

      updateNodeLevel: (id, level) => set((state) => ({
        nodes: state.nodes.map(n => n.id === id ? { ...n, level } : n),
        knowledgeGraph: {
          nodes: state.knowledgeGraph.nodes.map(n => n.id === id ? { ...n, level } : n),
          links: state.knowledgeGraph.links
        }
      })),

      loadGaps: () => {
        const nodes = get().nodes
        const domains = ['算法', '系统设计', 'C/C++', '数据库', '计算机网络']
        const gaps: Record<string, DomainLevel> = {}

        domains.forEach(domain => {
          const domainNodes = nodes.filter(n => n.domain === domain)
          const avgLevel = domainNodes.length > 0
            ? Math.round(domainNodes.reduce((acc, n) => acc + n.level, 0) / domainNodes.length * 20)
            : 0
          gaps[domain] = { level: avgLevel, count: domainNodes.length }
        })

        set({
          gaps: {
            algorithm: gaps['算法'],
            system: gaps['系统设计'],
            cpp: gaps['C/C++'],
            database: gaps['数据库'],
            network: gaps['计算机网络']
          }
        })
      }
    }),
    {
      name: 'knowledge-storage',
      version: 2,
      migrate: (persistedState: any, version: number) => {
        // v1 → v2：清空硬编码的 gaps 初始值（让用户从真实学习数据中积累）
        if (version < 2 && persistedState) {
          return {
            ...persistedState,
            gaps: {}
          }
        }
        return persistedState
      }
    }
  )
)
