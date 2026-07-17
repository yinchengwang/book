/**
 * 学习路径 Store
 *
 * 数据结构：
 * - Path (路径) → Step (大点) → SubStep (小点)
 * - 每 step 包含 3-6 个 sub-step（knowledge / practice / project 三种类型）
 * - 完成度 = sub-step 完成数 / sub-step 总数
 */
import { create } from 'zustand'
import { persist } from 'zustand/middleware'

// Sub-step 类型：知识点 / 实战练习 / 项目
export type SubStepType = 'knowledge' | 'practice' | 'project'

// Sub-step 数据
export interface SubStep {
  id: string
  title: string
  type: SubStepType
  estimatedHours: number
  refId?: string  // 关联到题库 / 摘抄 / 学习路径
  done?: boolean
}

// 学习步骤
export interface LearningStep {
  id: string
  title: string
  description: string
  difficulty: '入门' | '初级' | '中级' | '高级'
  estimatedHours: number
  resources: string[]
  subSteps: SubStep[]  // 大点拆小点
}

// 学习路径
export interface LearningPath {
  id: string
  name: string
  description: string
  icon: string
  difficulty: '入门' | '初级' | '中级' | '高级' | '专家'
  estimatedHours: number
  steps: LearningStep[]
  currentStep: number
}

// Store 接口
interface LearningPathState {
  paths: LearningPath[]
  activePathId: string | null
  activePath: LearningPath | null
  currentStep: number
  // 新字段：sub-step 完成状态（key: `${stepId}:${subStepId}`）
  completedSubSteps: Record<string, boolean>

  setActivePath: (id: string) => void
  nextStep: () => void
  prevStep: () => void
  resetPath: () => void
  toggleSubStep: (stepId: string, subStepId: string) => void
  getStepProgress: (stepId: string) => { done: number; total: number; percent: number }
  getPathProgress: (pathId: string) => { done: number; total: number; percent: number }
}

// 初始学习路径数据
const initialPaths: LearningPath[] = [
  {
    id: 'cpp-master',
    name: 'C++ 从入门到精通',
    description: '系统学习 C++，覆盖核心概念到高级特性',
    icon: '🚀',
    difficulty: '中级',
    estimatedHours: 80,
    currentStep: 0,
    steps: [
      {
        id: 'cpp-step-1',
        title: 'C++ 基础语法',
        description: '变量、数据类型、运算符、控制流、函数基础',
        difficulty: '入门',
        estimatedHours: 8,
        resources: ['C++ Primer 第1-3章', 'cppreference 基础部分'],
        subSteps: [
          { id: 'cpp-1-1', title: '变量与数据类型', type: 'knowledge', estimatedHours: 1.5 },
          { id: 'cpp-1-2', title: '运算符与表达式', type: 'knowledge', estimatedHours: 1 },
          { id: 'cpp-1-3', title: '控制流（if/for/while）', type: 'knowledge', estimatedHours: 1.5 },
          { id: 'cpp-1-4', title: '函数基础（声明/定义/调用）', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-1-5', title: '编写 Hello World 程序', type: 'practice', estimatedHours: 1 },
          { id: 'cpp-1-6', title: '实现简单计算器', type: 'project', estimatedHours: 1 }
        ]
      },
      {
        id: 'cpp-step-2',
        title: '面向对象编程',
        description: '类、对象、继承、封装、多态、虚函数',
        difficulty: '初级',
        estimatedHours: 12,
        resources: ['C++ Primer 第4-9章', '《Effective C++》'],
        subSteps: [
          { id: 'cpp-2-1', title: '类与对象', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-2-2', title: '构造函数与析构函数', type: 'knowledge', estimatedHours: 1.5 },
          { id: 'cpp-2-3', title: '继承与派生', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-2-4', title: '多态与虚函数', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-2-5', title: '运算符重载', type: 'knowledge', estimatedHours: 1.5 },
          { id: 'cpp-2-6', title: '实现 Shape 继承体系', type: 'project', estimatedHours: 3 }
        ]
      },
      {
        id: 'cpp-step-3',
        title: 'STL 标准模板库',
        description: '容器、迭代器、算法、函数对象、智能指针',
        difficulty: '初级',
        estimatedHours: 16,
        resources: ['《STL源码剖析》', 'cppreference STL部分'],
        subSteps: [
          { id: 'cpp-3-1', title: 'vector / list / deque', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-3-2', title: 'map / set / unordered_map', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-3-3', title: '迭代器与算法', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-3-4', title: '函数对象与 lambda', type: 'knowledge', estimatedHours: 1.5 },
          { id: 'cpp-3-5', title: '智能指针（unique/shared/weak）', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-3-6', title: '刷 30 道 STL 实战题', type: 'practice', estimatedHours: 6 }
        ]
      },
      {
        id: 'cpp-step-4',
        title: 'C++11/14/17 新特性',
        description: 'Lambda、移动语义、auto、范围for、并发编程',
        difficulty: '中级',
        estimatedHours: 20,
        resources: ['《现代C++语言核心特性》', 'C++ 标准文档'],
        subSteps: [
          { id: 'cpp-4-1', title: 'auto 与 decltype', type: 'knowledge', estimatedHours: 1.5 },
          { id: 'cpp-4-2', title: '右值引用与移动语义', type: 'knowledge', estimatedHours: 3 },
          { id: 'cpp-4-3', title: 'Lambda 表达式深入', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-4-4', title: '范围 for 与初始化列表', type: 'knowledge', estimatedHours: 1 },
          { id: 'cpp-4-5', title: 'std::thread 与并发', type: 'knowledge', estimatedHours: 4 },
          { id: 'cpp-4-6', title: '实现线程池', type: 'project', estimatedHours: 8 }
        ]
      },
      {
        id: 'cpp-step-5',
        title: '内存管理与性能优化',
        description: '内存模型、RAII、内存池、性能分析工具',
        difficulty: '中级',
        estimatedHours: 12,
        resources: ['《深度探索C++对象模型》', 'Valgrind 使用教程'],
        subSteps: [
          { id: 'cpp-5-1', title: '对象内存布局', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-5-2', title: 'RAII 与智能指针', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-5-3', title: '自定义内存分配器', type: 'knowledge', estimatedHours: 3 },
          { id: 'cpp-5-4', title: 'Valgrind / ASan 调试', type: 'practice', estimatedHours: 3 },
          { id: 'cpp-5-5', title: '性能优化实战', type: 'project', estimatedHours: 2 }
        ]
      },
      {
        id: 'cpp-step-6',
        title: '设计模式与架构',
        description: '常用设计模式、代码重构、架构设计原则',
        difficulty: '高级',
        estimatedHours: 12,
        resources: ['《设计模式》GoF', '《重构》Martin Fowler'],
        subSteps: [
          { id: 'cpp-6-1', title: '单例 / 工厂 / 观察者', type: 'knowledge', estimatedHours: 3 },
          { id: 'cpp-6-2', title: '策略 / 模板方法', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-6-3', title: '装饰器 / 代理模式', type: 'knowledge', estimatedHours: 2 },
          { id: 'cpp-6-4', title: '实现 EventBus 系统', type: 'project', estimatedHours: 5 }
        ]
      }
    ]
  },
  {
    id: 'algorithm',
    name: '算法与数据结构',
    description: '从基础到高级算法，LeetCode 高频题型',
    icon: '🧠',
    difficulty: '中级',
    estimatedHours: 100,
    currentStep: 0,
    steps: [
      {
        id: 'algo-step-1',
        title: '基础数据结构',
        description: '数组、链表、栈、队列、哈希表',
        difficulty: '入门',
        estimatedHours: 12,
        resources: ['《算法导论》第1-3章', 'LeetCode 数组/链表专题'],
        subSteps: [
          { id: 'algo-1-1', title: '数组操作与双指针', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-1-2', title: '链表反转 / 合并', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-1-3', title: '栈的应用（括号/表达式）', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-1-4', title: '队列与 BFS', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-1-5', title: '哈希表原理', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-1-6', title: '刷 20 道基础题', type: 'practice', estimatedHours: 2 }
        ]
      },
      {
        id: 'algo-step-2',
        title: '树与图',
        description: '二叉树、堆、图遍历、最短路、最小生成树',
        difficulty: '初级',
        estimatedHours: 16,
        resources: ['《算法导论》第4-6章', 'LeetCode 树/图专题'],
        subSteps: [
          { id: 'algo-2-1', title: '二叉树遍历', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-2-2', title: 'BST 与平衡树', type: 'knowledge', estimatedHours: 3 },
          { id: 'algo-2-3', title: '堆与优先队列', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-2-4', title: '图遍历（BFS/DFS）', type: 'knowledge', estimatedHours: 3 },
          { id: 'algo-2-5', title: '最短路（Dijkstra/Floyd）', type: 'knowledge', estimatedHours: 3 },
          { id: 'algo-2-6', title: '刷 25 道树图题', type: 'practice', estimatedHours: 3 }
        ]
      },
      {
        id: 'algo-step-3',
        title: '排序与搜索',
        description: '快速排序、归并排序、二分搜索、二叉搜索树',
        difficulty: '初级',
        estimatedHours: 12,
        resources: ['《算法导论》第7-8章', 'LeetCode 二分/排序专题'],
        subSteps: [
          { id: 'algo-3-1', title: '快速 / 归并排序', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-3-2', title: '堆排序 / 基数排序', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-3-3', title: '二分搜索模板', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-3-4', title: '旋转排序数组', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-3-5', title: '刷 20 道排序题', type: 'practice', estimatedHours: 4 }
        ]
      },
      {
        id: 'algo-step-4',
        title: '动态规划',
        description: 'DP 基础、状态压缩、树形DP、区间DP',
        difficulty: '中级',
        estimatedHours: 20,
        resources: ['《算法导论》第15章', 'LeetCode DP 专题（50题）'],
        subSteps: [
          { id: 'algo-4-1', title: 'DP 入门（斐波那契/爬楼梯）', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-4-2', title: '背包问题（01/完全）', type: 'knowledge', estimatedHours: 4 },
          { id: 'algo-4-3', title: '最长递增子序列', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-4-4', title: '区间 DP', type: 'knowledge', estimatedHours: 3 },
          { id: 'algo-4-5', title: '树形 DP', type: 'knowledge', estimatedHours: 3 },
          { id: 'algo-4-6', title: '状态压缩 DP', type: 'knowledge', estimatedHours: 3 },
          { id: 'algo-4-7', title: '刷 30 道 DP 难题', type: 'practice', estimatedHours: 3 }
        ]
      },
      {
        id: 'algo-step-5',
        title: '高级算法',
        description: '贪心、回溯、分治、位运算、字符串算法',
        difficulty: '中级',
        estimatedHours: 20,
        resources: ['《算法导论》第16-17章', 'LeetCode 贪心/回溯专题'],
        subSteps: [
          { id: 'algo-5-1', title: '贪心算法思想', type: 'knowledge', estimatedHours: 3 },
          { id: 'algo-5-2', title: '回溯算法模板', type: 'knowledge', estimatedHours: 4 },
          { id: 'algo-5-3', title: '分治思想', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-5-4', title: '位运算技巧', type: 'knowledge', estimatedHours: 2 },
          { id: 'algo-5-5', title: 'KMP / Z 函数', type: 'knowledge', estimatedHours: 3 },
          { id: 'algo-5-6', title: '刷 30 道高级题', type: 'practice', estimatedHours: 6 }
        ]
      },
      {
        id: 'algo-step-6',
        title: '面试冲刺',
        description: '高频面试题、面试技巧、算法思维训练',
        difficulty: '高级',
        estimatedHours: 20,
        resources: ['《剑指Offer》', 'LeetCode 热题 100'],
        subSteps: [
          { id: 'algo-6-1', title: 'LRU / LFU 缓存', type: 'practice', estimatedHours: 3 },
          { id: 'algo-6-2', title: 'Trie / 并查集', type: 'practice', estimatedHours: 3 },
          { id: 'algo-6-3', title: '单调栈 / 队列', type: 'practice', estimatedHours: 3 },
          { id: 'algo-6-4', title: 'TopK / 中位数', type: 'practice', estimatedHours: 3 },
          { id: 'algo-6-5', title: '模拟面试 10 场', type: 'project', estimatedHours: 8 }
        ]
      }
    ]
  },
  {
    id: 'system-design',
    name: '系统设计',
    description: '分布式系统、高并发、数据库设计',
    icon: '🏗️',
    difficulty: '高级',
    estimatedHours: 60,
    currentStep: 0,
    steps: [
      {
        id: 'sys-step-1',
        title: '计算机网络',
        description: 'TCP/IP、HTTP/HTTPS、WebSocket、DNS',
        difficulty: '中级',
        estimatedHours: 12,
        resources: ['《计算机网络》谢希仁', '《HTTP权威指南》'],
        subSteps: [
          { id: 'sys-1-1', title: 'OSI 七层模型', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-1-2', title: 'TCP 三次握手 / 四次挥手', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-1-3', title: 'HTTP / HTTPS 详解', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-1-4', title: 'DNS / DHCP', type: 'knowledge', estimatedHours: 1.5 },
          { id: 'sys-1-5', title: '用 Wireshark 抓包分析', type: 'practice', estimatedHours: 2 },
          { id: 'sys-1-6', title: '实现简易 HTTP 服务器', type: 'project', estimatedHours: 2.5 }
        ]
      },
      {
        id: 'sys-step-2',
        title: '操作系统',
        description: '进程线程、内存管理、文件系统、IO模型',
        difficulty: '中级',
        estimatedHours: 16,
        resources: ['《操作系统概念》', '《Linux内核设计与实现》'],
        subSteps: [
          { id: 'sys-2-1', title: '进程与线程', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-2-2', title: '内存管理与虚拟内存', type: 'knowledge', estimatedHours: 3 },
          { id: 'sys-2-3', title: '文件系统', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-2-4', title: 'IO 多路复用（epoll）', type: 'knowledge', estimatedHours: 3 },
          { id: 'sys-2-5', title: '进程间通信', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-2-6', title: '实现生产者消费者', type: 'project', estimatedHours: 4 }
        ]
      },
      {
        id: 'sys-step-3',
        title: '数据库原理',
        description: 'SQL、索引、事务、锁、日志、缓存',
        difficulty: '中级',
        estimatedHours: 16,
        resources: ['《数据库系统概念》', '《高性能MySQL》'],
        subSteps: [
          { id: 'sys-3-1', title: 'SQL 进阶（JOIN/子查询）', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-3-2', title: '索引原理（B+ Tree）', type: 'knowledge', estimatedHours: 3 },
          { id: 'sys-3-3', title: '事务与隔离级别', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-3-4', title: '锁机制（行锁/表锁）', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-3-5', title: '日志（Redo/Undo）', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-3-6', title: '实现简易 ORM', type: 'project', estimatedHours: 5 }
        ]
      },
      {
        id: 'sys-step-4',
        title: '分布式系统',
        description: '一致性、CAP、BASE、共识算法、微服务',
        difficulty: '高级',
        estimatedHours: 16,
        resources: ['《分布式系统设计》', '《Designing Data-Intensive Applications》'],
        subSteps: [
          { id: 'sys-4-1', title: 'CAP / BASE 理论', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-4-2', title: '一致性协议（2PC/3PC）', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-4-3', title: '共识算法（Raft/Paxos）', type: 'knowledge', estimatedHours: 4 },
          { id: 'sys-4-4', title: '微服务架构', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-4-5', title: '分布式 ID / 锁', type: 'knowledge', estimatedHours: 2 },
          { id: 'sys-4-6', title: '设计短链服务', type: 'project', estimatedHours: 4 }
        ]
      }
    ]
  }
]

export const useLearningPathStore = create<LearningPathState>()(
  persist(
    (set, get) => ({
      paths: initialPaths,
      activePathId: null,
      activePath: null,
      currentStep: 0,
      completedSubSteps: {},

      // 选择路径
      setActivePath: (id) => {
        const path = get().paths.find(p => p.id === id)
        if (path) {
          set({
            activePathId: id,
            activePath: path,
            currentStep: path.currentStep
          })
        }
      },

      // 下一步
      nextStep: () => {
        const { activePath, currentStep } = get()
        if (activePath && currentStep < activePath.steps.length - 1) {
          const newStep = currentStep + 1
          const updatedPaths = get().paths.map(p =>
            p.id === activePath.id ? { ...p, currentStep: newStep } : p
          )
          set({
            currentStep: newStep,
            paths: updatedPaths,
            activePath: { ...activePath, currentStep: newStep }
          })
        }
      },

      // 上一步
      prevStep: () => {
        const { activePath, currentStep } = get()
        if (activePath && currentStep > 0) {
          const newStep = currentStep - 1
          const updatedPaths = get().paths.map(p =>
            p.id === activePath.id ? { ...p, currentStep: newStep } : p
          )
          set({
            currentStep: newStep,
            paths: updatedPaths,
            activePath: { ...activePath, currentStep: newStep }
          })
        }
      },

      // 重置路径
      resetPath: () => {
        const { activePath } = get()
        if (activePath) {
          const updatedPaths = get().paths.map(p =>
            p.id === activePath.id ? { ...p, currentStep: 0 } : p
          )
          set({
            currentStep: 0,
            paths: updatedPaths,
            activePath: { ...activePath, currentStep: 0 }
          })
        }
      },

      // 切换 sub-step 完成状态
      toggleSubStep: (stepId, subStepId) => {
        const key = `${stepId}:${subStepId}`
        const completed = { ...get().completedSubSteps }
        if (completed[key]) {
          delete completed[key]
        } else {
          completed[key] = true
        }
        set({ completedSubSteps: completed })
      },

      // 获取 step 进度
      getStepProgress: (stepId) => {
        const path = get().activePath || get().paths.find(p =>
          p.steps.some(s => s.id === stepId)
        )
        if (!path) return { done: 0, total: 0, percent: 0 }
        const step = path.steps.find(s => s.id === stepId)
        if (!step || step.subSteps.length === 0) return { done: 0, total: 0, percent: 0 }
        const completed = get().completedSubSteps
        const done = step.subSteps.filter(s => completed[`${stepId}:${s.id}`]).length
        return {
          done,
          total: step.subSteps.length,
          percent: Math.round((done / step.subSteps.length) * 100)
        }
      },

      // 获取整个路径的进度
      getPathProgress: (pathId) => {
        const path = get().paths.find(p => p.id === pathId)
        if (!path) return { done: 0, total: 0, percent: 0 }
        const completed = get().completedSubSteps
        let total = 0
        let done = 0
        for (const step of path.steps) {
          total += step.subSteps.length
          for (const sub of step.subSteps) {
            if (completed[`${step.id}:${sub.id}`]) done++
          }
        }
        return {
          done,
          total,
          percent: total > 0 ? Math.round((done / total) * 100) : 0
        }
      }
    }),
    {
      name: 'learning-path-storage',
      version: 2,
      migrate: (persistedState: any, version: number) => {
        // v1 → v2：补全 subSteps 字段（老数据无此字段）
        // 同时清空老 activePath 引用，避免老 step（无 subSteps）渲染时崩
        if (version < 2 && persistedState?.paths) {
          return {
            // 直接使用新版 initialPaths 替换
            paths: initialPaths,
            // 强制清空 activePath / activePathId：避免指向老 step 数组（缺 subSteps 字段）
            // 触发 LearningPath.tsx:113 的 reduce(s.subSteps.length) 崩溃
            activePath: null,
            activePathId: null,
            currentStep: 0,
            // 保留用户老 sub-step 完成状态（如有）
            completedSubSteps: persistedState.completedSubSteps || {}
          }
        }
        return persistedState
      }
    }
  )
)