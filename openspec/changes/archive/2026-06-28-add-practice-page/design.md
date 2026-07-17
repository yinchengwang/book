## Context

Reading Radar 是一个纯静态 HTML/JS 项目（无构建工具），所有页面使用全局变量 + localStorage 持久化。现有 7 个页面共享统一的导航组件和数据架构（items-registry.js → radar-tech.js/kanban-data.js/quiz-tech.js）。新增页面需要遵循现有模式：新建 HTML 文件 + 数据 JS 文件 + 注册导航。

刷题计划.md 中已有约 100 道题的原始计划，但需求扩展为覆盖所有数据结构和算法的完整课程体系（仅 Easy/Medium），按知识点分类 + 严格 Easy→Medium 排序。

## Goals / Non-Goals

**Goals:**
- 建立一个覆盖所有 DS&A 知识点的刷题课程体系（约 20+ 分类，200+ 题）
- 提供分阶段折叠导航的分组浏览（6 个阶段）
- 每道题支持复选框完成状态跟踪（localStorage 持久化）
- 遵循现有项目架构：纯静态 HTML/JS，无构建工具，全局变量模式

**Non-Goals:**
- 不实现 Canvas 雷达图可视化（采用列表式页面，类似 interview.html）
- 不包含 Hard 难度题目
- 不包含代码编辑/运行功能
- 不包含自动推荐/排期功能

## Decisions

### 1. 数据文件采用独立数组结构（而非扩展 items-registry.js）

**选择**：创建独立的 `data/app/practice-data.js`，定义 `PRACTICE_CATEGORIES` 全局数组。

**原因**：刷题计划的知识点分类与 items-registry 的 stack/quadrant 体系正交，强行塞入会破坏现有架构。独立文件可以自由定义分类名、阶段、平台、难度等刷题特定字段。

### 2. 状态存储使用 localStorage + key-value 模式

**选择**：以 `practice_<problemId>` 或 `practice_status` 为 key，存储已完成题目的 ID 集合。

**原因**：与项目现有的状态管理模式一致（如阅读状态、测验分数均使用 localStorage），无需引入外部依赖。

### 3. 页面布局采用 interview.html 模式（左侧导航 + 右侧列表）

**选择**：左侧固定宽度分类导航（分阶段折叠），右侧显示当前分类题目列表。

**原因**：interview.html 已在同一项目中验证了该布局的可用性，CSS 样式可复用。用户已明确选择方式 A（按知识点分类列表）。

### 4. 每个分类的题目使用扁平数组 + 严格 Easy→Medium 排序

**选择**：数据文件中每个分类的 items 数组按 Easy→Medium 顺序预先排好，页面渲染时直接依序遍历输出。

**原因**：避免运行时排序逻辑，数据即展示顺序。用户要求的"严格 Easy→Medium"在数据层面固化。

### 5. 牛客和 LeetCode 混排，用标签区分平台

**选择**：同一知识点分类下同时包含 LeetCode 和牛客题目，按难度混在一起。每道题标注平台来源（显示 LC 编号或 牛客编号）。

**原因**：用户明确要求混排。两平台题目互补，LeetCode 偏算法思想，剑指Offer 偏面试实战。

## 数据架构

```
data/app/practice-data.js
└── PRACTICE_CATEGORIES: Array<{
      id: string,         // 唯一标识，如 "array", "two-pointers"
      title: string,      // 显示名，如 "数组", "双指针"
      phase: number,      // 阶段编号 1-6
      order: number,      // 阶段内排序
      icon: string,       // Emoji 图标
      desc: string,       // 简短描述
      items: Array<{
        title: string,     // 题目名称
        platform: string,  // "leetcode" | "nowcoder"
        problemId: number | string,  // LeetCode 题号 / 牛客编号
        difficulty: string, // "easy" | "medium"
        tags: string[],    // 标签，如 ["双指针", "哈希表"]
        leetcodeSlug?: string,  // LeetCode URL slug（可选）
      }>
    }>
```

## 六个阶段设计

| 阶段 | 名称 | 分类数 | 难度曲线 |
|------|------|--------|---------|
| 1 | 基础数据结构 | 5 类（数组/字符串/链表/栈/队列） | Easy 入门 |
| 2 | 算法思想入门 | 5 类（双指针/滑动窗口/前缀和/哈希表/二分） | Easy→Medium 过渡 |
| 3 | 排序与树 | 3 类（排序/二叉树/BST） | Easy→Medium |
| 4 | 搜索与回溯 | 3 类（DFS/BFS/回溯） | Medium 为主 |
| 5 | 算法进阶 | 5 类（贪心/DP入门/DP进阶/堆/并查集） | Easy→Medium 进阶 |
| 6 | 图论与综合 | 4 类（图论/位运算/数学/面试经典） | Medium 收尾 |

## 页面状态管理

- 复选框状态 key: `practice_done_<categoryId>_<itemIndex>` 或统一 key `practice_done` 存 JSON 对象
- 加载时读取 localStorage 恢复勾选状态
- 勾选时即时写入 localStorage
- 统计显示：当前分类已做/总数 + 全局已做/总数

## Risks / Trade-offs

- [数据量大] 手动录入 200+ 道题较耗时 → 可以按阶段分批录入，先完成阶段 1-2 的核心数据再增补
- [硬编码] 题目数据全部硬编码在 JS 中，更新需要改源码 → 纯静态项目的常态，与现有数据模式一致
- [无搜索] 当前设计不包含搜索功能 → 题量 200+ 时可能不便，可在后续迭代中增加搜索

## Open Questions

- 需要最终确认每个分类的精确题数分配和各分类间的边界划分
