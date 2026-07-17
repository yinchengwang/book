## Context

当前图解系列页面采用单列卡片列表布局，所有文章按 6 个扁平分类展示。其中"综合"分类包含 154 篇主题各异的文章（Linux、网络、系统概念等），导致分类粒度不足，用户难以定位目标内容。

**现状：**
- 列表模式：顶部筛选器 + 卡片列表
- 详情模式：面包屑 + 文章内容 + 上/下篇
- 分类：综合(154)、数据库(61)、算法(52)、C(44)、Python(43)、C++(42)

**约束：**
- 需要同时支持 H5 和小程序端渲染
- Markdown 渲染组件已实现（react-markdown）
- 396 篇文章数据存储在 TS 文件中

## Goals / Non-Goals

**Goals:**
1. 实现左侧分类目录 + 右侧文章内容的双栏布局
2. 将"综合"分类拆分为 Linux、网络、系统三个独立分类
3. 重构数据库分类为"传统数据库"和"向量数据库"
4. 支持文章上/下篇导航
5. 保留搜索和分类筛选功能

**Non-Goals:**
- 不改变 Markdown 渲染组件（已实现）
- 不修改文章内容存储格式（继续使用 TS + Markdown）
- 不涉及其他页面（如 quiz、interview_bank 等）
- 小程序端布局可保持简单列表，不强制双栏

## Decisions

### Decision 1: 分类体系重构

**选择：** 按主题领域划分 9 个一级分类

```
Linux（~80篇）| 网络（~50篇）| 系统（~30篇）
数据库（~30篇）| 向量数据库（~30篇）| 算法（~52篇）
C（~44篇）| Python（~43篇）| C++（~42篇）
```

**替代方案：**
- 保留扁平分类，仅拆分"综合" → 分类仍过多
- 二级分类树 → 复杂度增加，交互层级过深

**理由：** 一级分类足够细分，用户可通过折叠/展开快速定位。

### Decision 2: 页面布局

**选择：** 左侧固定目录栏（~250px）+ 右侧自适应内容区

```
┌──────────────────────────────────────────────────┐
│  图解系列                    [搜索框]              │
├──────────────┬───────────────────────────────────┤
│  ▼ Linux     │                                   │
│    基础知识   │  文章标题                         │
│    文件系统   │  ─────────                        │
│  ▶ 网络      │  Markdown 内容...                  │
│  ▶ 系统      │                                   │
│  ▶ 数据库    │                                   │
│  ...         │  ← 上一篇    下一篇 →             │
└──────────────┴───────────────────────────────────┘
```

**H5 端：** CSS Grid/Flexbox 实现双栏，目录栏固定宽度
**小程序端：** 保持简单列表模式（目录模式对小屏不友好）

### Decision 3: 分类导航数据结构

**选择：** 在 `learn_deep_index.ts` 中新增 `categoryOrder` 常量定义分类顺序

```typescript
export const CATEGORY_ORDER = ['linux', 'network', 'system', 'database', 'vdb', 'algorithm', 'c', 'python', 'cpp'] as const

export type CategoryId = typeof CATEGORY_ORDER[number]

export interface CategoryMeta {
  id: CategoryId
  name: string          // 显示名称：'Linux'
  slug: string           // URL slug：'linux'
  parentId?: CategoryId  // 父分类（支持二级）
  count: number          // 该分类文章数
}

export interface LearnDeepSummary {
  slug: string
  title: string
  categoryId: CategoryId  // 改为 categoryId
  // ...
}
```

### Decision 4: 文章路由

**选择：** 保持当前 URL 结构，仅在客户端切换显示

```
/learn_deep           → 列表模式
/learn_deep/:slug     → 详情模式（URL 参数）
```

通过 React state 控制左侧目录是否显示，不依赖 URL。

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| 396 篇文章重新分类工作量大 | 中 | 提供分类映射表脚本，半自动迁移 |
| 小程序端双栏布局不友好 | 低 | 小程序保持简单列表，不强制双栏 |
| 分类边界模糊的文章归属 | 低 | 优先归属主要主题，允许重复分类 |
| 搜索性能（396篇文章） | 低 | 客户端过滤，数据量可接受 |

## Migration Plan

### Phase 1: 数据迁移
1. 创建分类映射脚本
2. 执行 TS 文件更新
3. 验证分类数量正确

### Phase 2: 组件重构
1. 创建新的 `LearnDeepLayout` 组件
2. 实现 `CategorySidebar` 组件
3. 重构 `ArticleReader` 组件

### Phase 3: 集成测试
1. H5 端双栏布局验证
2. 上/下篇导航验证
3. 搜索功能验证

### Phase 4: 小程序适配
1. 评估小程序端体验
2. 可选：实现简化版布局

## Open Questions

1. **分类边界：** "网络"和"系统"有重叠（如 TCP 属于网络还是系统？）→ 按主要内容主题归类
2. **二级分类：** 是否需要？→ 本次先不实现，保持一级分类
3. **文章数量显示：** 目录栏是否显示每个分类的文章数？→ 是，提升信息密度
