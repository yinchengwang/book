# 设计文档：reading-radar 升级

## Context

reading-radar 是纯静态 HTML/JS 学习站点，无构建工具，6 个 HTML 页面 + 1 个 Node 服务 + ~30 个数据/逻辑 JS 文件。全局变量挂载模式（`window` 下共享数据），localStorage 持久化状态。

当前学习内容页 `learn.html` 使用 `data/learn-content-<catId>.js` 中的 JavaScript 全局变量存储四段式模板内容（入门/原理/实战/调优），通过 `innerHTML` 拼接渲染。300 个知识点已有 `.md` 源文件存放在 `data/learn-deep/{stack}/{quadrant}/{id}.md`，但未被 `learn.html` 引用。

数据库内容混放了通用架构知识（11 个）和产品专属知识（MySQL 23 个、Redis 6 个等共 48 个），向量数据库内容（15 个 faiss/diskann/milvus/pinecone）也散落在 db 栈中。

## Goals / Non-Goals

**Goals:**
- `learn.html` 改为通过 Markdown 解析器渲染 `.md` 文件，替代 JS 模板引擎
- 图解图片穿插在章节之间，居中显示并附说明文字
- 数据库内容重组：db 栈仅保留通用架构，产品专属迁移到 `illustrate/` 目录
- 向量数据库内容从 db 栈迁移到 illustrate 独立产品目录
- learn.html 视觉升级：流式长文布局、侧边栏滚动高亮
- index.html 精简：去掉嵌入看板

**Non-Goals:**
- 不改变 index.html 的雷达图逻辑（仅去掉看板嵌入）
- 不改变 learning-kanban.html 看板逻辑
- 不改变 quiz-system.html 测验系统
- 不改变 dashboard.html 仪表板
- 不创建新的构建工具或打包流程
- 不修改 items-registry.js 的知识点元数据（id/title/desc/tags 不变）

## Decisions

### 决策 1：使用 marked.js 作为 Markdown 解析器

**选择**: marked.js（~30KB，CDN 加载）

**理由**:
- 项目是纯静态 HTML/JS，无 Node.js 构建步骤，无法使用服务端渲染
- marked.js 是最轻量成熟的浏览器端 Markdown 解析器
- 支持 GFM 表格、代码块语言标注、图片等全部需要的语法
- 30KB 体积对静态站点影响极小

**替代方案**:
- `showdown.js`（~40KB）— 功能类似，marked 更流行、社区更大
- `remark.js`（~200KB）— 太重，需要插件链
- 手写正则解析 — 不可维护，不支持嵌套语法

### 决策 2：MD 文件路径映射规则

**选择**: `data/learn-deep/{stack}/{quadrant}/{id}.md`，vdb 栈映射到 db 目录

**理由**:
- C/CPP/DS/PY/LINUX 栈各自独立目录，直观对应 items-registry.js 的 stack 字段
- VDB 栈（向量数据库）复用 db 目录结构（因为 items-registry.js 中 vdb 产品的文件已经在 db 目录下），只需在解析时识别 `stack === 'vdb'` 映射到 `db/` 目录
- 图解系列（illustrate）独立于技术栈，按产品名组织

**路径映射表**:
```
stack=c  → data/learn-deep/c/{quadrant}/{id}.md
stack=cpp → data/learn-deep/cpp/{quadrant}/{id}.md
stack=ds  → data/learn-deep/ds/{quadrant}/{id}.md
stack=db  → data/learn-deep/db/{quadrant}/{id}.md
stack=py  → data/learn-deep/py/{quadrant}/{id}.md
stack=linux → data/learn-deep/linux/{quadrant}/{id}.md
stack=vdb → data/learn-deep/db/{quadrant}/{id}.md  (vdb 复用 db 目录)
```

**Illustrate 系列路径**:
```
data/learn-deep/illustrate/{product}/{id}.md
```

### 决策 3：图解图片存放策略

**选择**: 每个知识点同级目录下 `diagrams/` 子目录存放图解图片

**理由**:
- 相对路径引用（`./diagrams/xxx.png`），与 MD 文件天然关联
- 便于版本管理和文件迁移（MD 和图片始终在一起）
- 浏览器原生支持 `<img>` 标签，marked.js 无需额外插件

**替代方案**:
- 集中存放 `data/learn-deep/images/` — 文件多了难以对应
- SVG 内联在 MD 中 — 维护困难，Obsidian 渲染不一致

### 决策 4：file:// 协议降级策略

**选择**: 检测 `location.protocol === 'file:'` 时显示友好提示，不尝试加载

**理由**:
- `fetch()` 在 file:// 协议下被浏览器安全策略阻止（CORS）
- 项目已有 `node server.js` 本地服务器，用户应通过服务器访问
- 强行支持 file:// 需要引入 XMLHttpRequest 或其他 hack，增加复杂度

### 决策 5：侧边栏滚动高亮使用 IntersectionObserver

**选择**: IntersectionObserver API

**理由**:
- 比 scroll 事件监听性能更好（浏览器原生优化，不触发重排）
- 可精确控制触发阈值（`rootMargin: '-20% 0px -60% 0px'`）
- 现代浏览器全部支持，无需 polyfill

**替代方案**:
- scroll 事件 + scrollIntoView — 性能差，频繁触发
- IntersectionObserver 是业界标准方案

### 决策 6：数据库内容重组不修改 items-registry.js 的 stack/product 映射

**选择**: items-registry.js 中 db 相关知识点保持 `stack=db` 和 `product=mysql/redis/...` 不变

**理由**:
- 现有 quiz-tech.js、radar-tech.js、kanban-data.js 三个消费者文件依赖 stack/product 字段
- 修改 stack/product 会导致三个消费者文件需要同步修改，风险大
- 内容文件存放位置（文件系统）与数据模型的 stack/product 字段是正交的
- 向量数据库（vdb 栈）已在 items-registry.js 中正确标记，只需调整文件存放路径

## Risks / Trade-offs

| Risk | 影响 | 缓解措施 |
|------|------|---------|
| `fetch()` 加载 MD 文件需要本地服务器 | 用户双击 HTML 无法使用 | 显示友好提示引导用户使用 `node server.js` |
| 300 个 MD 文件需要内容风格改造 | 工作量巨大 | 分批进行，先做 5 个示范，后续用 AI 辅助批量改写 |
| 图解图片需要手动创作 | 初期无图解 | 先用纯文字内容上线，图解逐步补充 |
| 数据库内容迁移可能遗漏文件 | 部分知识点内容丢失 | 通过脚本自动迁移，迁移后对比 MD 文件数量和 ID 覆盖率 |
| marked.js CDN 加载在网络受限环境失败 | 无法解析 Markdown | 提供本地 marked.min.js 作为 fallback |

## Migration Plan

### 阶段 1：基础设施（不破坏现有功能）
1. 创建 `data/learn-deep/illustrate/` 目录结构
2. 迁移数据库产品专属内容到 illustrate 目录
3. 修正 items-registry.js 中 vdb 产品的 stack/product 映射
4. 在 learn.html 引入 marked.js（保留原有 JS 模板渲染逻辑）

### 阶段 2：learn.html 改造（双模式运行）
5. 在 learn.html 中新增 Markdown 渲染路径（检测 `.md` 文件是否存在）
6. 如果 MD 文件存在 → 走 Markdown 渲染路径
7. 如果 MD 文件不存在 → 回退到原有 JS 模板渲染路径
8. 测试所有 300 个知识点的加载

### 阶段 3：内容迁移
9. 逐个技术栈迁移 MD 文件到新目录结构
10. 验证每个知识点的 MD 文件可正确加载
11. 删除 `data/learn-content-<catId>.js` 文件

### 阶段 4：视觉升级
12. learn.html 内容区加宽到 900px
13. 去掉四模块卡片布局，改为流式长文
14. 添加图解图片居中样式
15. 实现侧边栏滚动高亮
16. index.html 去掉嵌入看板

### 阶段 5：清理
17. 删除旧的 `data/learn-content-<catId>.js` 文件
18. 删除旧的 `data/learn-deep/db/` 子目录结构（algorithms/engineering/language/systems）
19. 更新 README.md

## Open Questions

1. **图解图片格式**：PNG（通用）还是 SVG（可缩放、可编辑）？PNG 更兼容，SVG 更适合手绘风格且可编辑。
2. **openGauss 内容**：目前没有 openGauss 知识点，是否需要从零创建？如果需要，预计多少个知识点？
3. **网络/操作系统/Agent/Claude Code 图解**：这些是全新内容，是否有现有素材可以迁移？还是需要从零创作？
4. **内容改造优先级**：300 个知识点分批改造，先改哪个技术栈？建议先改 C 语言（44 个），因为使用最频繁。
