## Why

当前图解系列页面（learn_deep）存在以下问题：
1. **分类混乱**："综合"分类包含 154 篇，涵盖 Linux 系统、计算机网络、系统概念等多种主题，用户难以快速找到目标文章
2. **布局不便**：所有文章以单列卡片列表展示，缺乏分类导航，用户需频繁滚动和搜索
3. **缺乏沉浸式阅读体验**：进入详情页后没有侧边目录辅助导航

## What Changes

### 1. 分类体系重构

将现有的 6 个分类（综合、数据库、算法、C、Python、C++）重新整理为 9 个清晰分类：

| 新分类 | 描述 | 文章数（估算） |
|--------|------|---------------|
| Linux | Linux 系统内核、文件系统、进程管理、性能工具 | ~80 篇 |
| 网络 | TCP/IP、HTTP、网络编程、I/O 模型 | ~50 篇 |
| 系统 | 进程线程、内存管理、CPU、锁机制 | ~30 篇 |
| 数据库 | MySQL/PostgreSQL 存储引擎、索引、事务 | ~30 篇 |
| 向量数据库 | Milvus、Pinecone、HNSW、向量索引 | ~30 篇 |
| 算法 | 排序、查找、图论、动态规划 | ~52 篇 |
| C | C 语言语法、指针、内存、进程线程 | ~44 篇 |
| Python | Python 语法、数据结构、元编程 | ~43 篇 |
| C++ | C++ 特性、智能指针、多线程 | ~42 篇 |

### 2. 页面布局重构

采用**左侧目录 + 右侧内容**的布局：

```
┌─────────────────────────────────────────────────────────────────┐
│  图解系列                                        [搜索] [筛选]    │
├──────────────┬──────────────────────────────────────────────────┤
│              │                                                  │
│  ▼ Linux     │  文章标题                                        │
│    基础知识   │  ─────────                                       │
│    文件系统   │  文章内容...                                     │
│    进程管理   │                                                  │
│  ▼ 网络      │  Lorem ipsum dolor sit amet, consectetur         │
│    TCP/IP    │  adipiscing elit. Sed do eiusmod tempor          │
│    HTTP      │  incididunt ut labore et dolore magna aliqua.    │
│  ▼ 算法      │                                                  │
│    排序      │  ─────────────────────────────────────────────    │
│    查找      │  ← 上一篇：xxx          下一篇：xxx →            │
│  ...         │                                                  │
│              │                                                  │
└──────────────┴──────────────────────────────────────────────────┘
```

### 3. 分类导航交互优化

- 分类以可折叠树形结构展示
- 点击分类展开/折叠子分类
- 点击文章标题直接加载文章内容到右侧
- 右侧底部显示上/下篇导航

### 4. 搜索功能增强

- 支持按标题、标签、分类搜索
- 搜索结果高亮显示匹配内容
- 实时过滤，无需提交

## Capabilities

### New Capabilities

- `learn-deep-categories`: 图解系列分类体系，包含分类树结构和文章归属定义
- `learn-deep-sidebar`: 左侧分类导航栏，支持折叠、搜索、高亮当前选中
- `learn-deep-reader`: 右侧文章阅读器，包含 Markdown 渲染和上/下篇导航
- `learn-deep-search`: 文章搜索功能，支持标题和标签全文搜索

### Modified Capabilities

- `learn-deep-index`: 更新分类数据结构，支持树形分类而非扁平列表

## Impact

### 受影响代码

- `src/pages/learn_deep/learn_deep.tsx` - 重写页面组件
- `src/pages/learn_deep/learn_deep.scss` - 新增布局样式
- `src/data/learn_deep_index.ts` - 更新分类元数据
- `src/data/learn_deep_content.ts` - 分类字段更新

### 数据迁移

- 396 篇文章需要重新分类标注
- 154 篇"综合"文章需要拆分到 Linux、网络、系统等分类

### 依赖

- 继续使用 `react-markdown` + `remark-gfm` + `react-syntax-highlighter` 进行 Markdown 渲染
- 继续使用 Zustand 进行状态管理
