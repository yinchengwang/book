# 设计文档：knowledge-hub 重构

## Context

### 当前状态

- 项目位于 `reading-radar-2/` 目录
- 部分路径使用中划线命名：`learn-deep`、`interview-tracker`、`mock-interview` 等
- 图解系列（396 篇）内容存储在 `src/data/learn-deep-content.ts`，渲染使用手写的简化解析器
- 路由路径使用中划线：`/learn-deep`、`/interview-tracker` 等

### 约束条件

1. **Git 历史保留**：目录重命名需保留 Git 历史
2. **双端兼容**：H5 和小程序端都需要正常工作
3. **用户书签**：旧路由路径失效需考虑重定向或提示
4. **构建验证**：每次变更后需验证 TypeScript 编译和构建通过
5. **CLAUDE.md 规范**：内容数据必须以 Markdown 文件存储

### 干系人

- 用户：依赖旧链接访问图解系列
- 开发者：依赖一致的文件命名规范

## Goals / Non-Goals

**Goals:**
- 统一项目命名为 `knowledge_hub`
- 所有文件名、目录名、路由路径使用下划线命名
- 图解系列使用功能完整的 Markdown 渲染器（支持图片、表格、代码高亮）
- 图解内容迁移到独立 `.md` 文件
- 双端（H5 + 小程序）均正常工作

**Non-Goals:**
- 不重构页面 UI 样式
- 不修改图解系列的数据内容（仅迁移格式）
- 不迁移其他数据模块（题库、摘抄等）到 MD 文件
- 不改变现有功能逻辑

## Decisions

### Decision 1: 目录重命名策略

**选项 A：先改目录名，再改内容中的引用**
```
1. git mv reading-radar-2 knowledge_hub
2. 批量替换所有 import/require 路径
```

**选项 B：先改内容引用，再改目录名**
```
1. 在 reading-radar-2 内完成所有文件重命名和引用更新
2. 最后一步 git mv 整个目录
```

**选择：A** - 理由：
- `git mv` 可以保留完整的 Git 历史
- 目录重命名后 Git 能自动追踪文件移动
- 最后一步改目录名，出错概率最低

### Decision 2: 文件名重命名范围

**选择：全部重命名** - 包括：
- 页面目录：`learn-deep/` → `learn_deep/`
- 组件文件：`LearnDeep.tsx` → `learn_deep.tsx`（大驼峰 → 小写下划线）
- 样式文件：`LearnDeep.scss` → `learn_deep.scss`
- 数据文件：`learn-deep-index.ts` → `learn_deep_index.ts`
- 脚本文件：`migrate-learn-deep.cjs` → `migrate_learn_deep.cjs`
- 测试文件：`learn-deep.spec.ts` → `learn_deep.spec.ts`

**不使用** `rename-imports` 或类似工具，手动确保每处引用都被正确更新。

### Decision 3: 路由重定向策略

**选择：硬切换 + 可选软重定向**

- 直接将路由从 `/learn-deep` 改为 `/learn_deep`
- 不做 URL 重定向（避免维护两套路由）
- 在项目 README 中注明路由变更，用户需更新书签

**理由**：这是一个新项目（未公开发布），用户基数小，路由重定向的复杂度不划算。

### Decision 4: Markdown 渲染方案

#### H5 端

```
技术栈：react-markdown + remark-gfm + react-syntax-highlighter
```

| 组件 | 用途 |
|------|------|
| `react-markdown` | 核心解析器 |
| `remark-gfm` | 支持 GFM 语法（表格、任务列表、删除线） |
| `react-syntax-highlighter` | 代码块语法高亮 |
| `rehype-raw` | 支持 HTML 标签透传（如 `<div>`） |

**组件结构**：
```
src/components/markdown/
├── MarkdownRenderer.tsx      # 主渲染组件
├── CodeBlock.tsx            # 代码块（语法高亮）
├── ImageFigure.tsx           # 图片（居中 + caption）
└── MarkdownRenderer.scss     # 样式
```

#### 小程序端

小程序不支持 npm 包直接使用，采用**自定义解析器**方案：

```
实现方式：手写 Markdown 解析器（兼容 H5 同款语法）
```

| 语法 | 渲染方式 |
|------|----------|
| 标题 | `View` + 字号样式 |
| 代码块 | `View` + 背景色 + 滚动 |
| 图片 | `Image` 组件 |
| 表格 | 嵌套 `View` 模拟表格 |
| 链接 | `navigator` 组件 |

**理由**：微信小程序的限制（不支持动态 style、无法使用任意 npm 包），需要针对性适配。

### Decision 5: 内容迁移策略

**迁移方式**：自动化脚本 + 人工验证

```
1. 创建迁移脚本：src/data/convert_to_md.ts
2. 解析 learn-deep-content.ts 中的每篇文章
3. 按分类输出到 src/data/learn-deep/{category}/{slug}.md
4. 生成 learn_deep_index.ts（纯索引，不含正文）
5. 人工抽检 5-10 篇验证格式正确
```

**文件命名规范**：
```
src/data/learn_deep/
├── c/
│   ├── c_syntax.md
│   ├── c_control_flow.md
│   └── ...
├── cpp/
│   └── ...
├── algorithm/
│   └── ...
└── ...
```

**slug 生成规则**：标题转小写，空格用下划线，移除特殊字符

## Risks / Trade-offs

### Risk 1: 文件重命名漏改引用

**风险**：手动重命名时可能遗漏某处 import 引用，导致运行时 404。

** Mitigation**：
- 使用 IDE 的全局搜索（`Ctrl+Shift+F`）搜索所有可能的旧路径模式
- 完成重命名后立即运行 `bunx tsc --noEmit` 验证
- 运行 `node web/scripts/test-pages.cjs` 冒烟测试

### Risk 2: Git 历史追踪丢失

**风险**：`git mv` 后 Git 可能无法正确追踪某些文件的移动历史。

**Mitigation**：
- 分阶段提交：先完成内容变更，最后一步改目录名
- 提交信息明确标注 `git mv`

### Risk 3: Markdown 渲染兼容性问题

**风险**：396 篇文章格式不统一，迁移后可能出现渲染异常。

**Mitigation**：
- 先抽样检查 10 篇内容格式
- 脚本迁移时记录异常文章列表
- H5 端使用 `react-markdown` 的 `remarkPlugins` 和 `rehypePlugins` 处理特殊情况

### Risk 4: 小程序端渲染性能

**风险**：自定义 Markdown 解析器在大段文章时可能性能不佳。

**Mitigation**：
- 使用 `rich-text` 组件或 `taroify` 的 Markdown 组件
- 分段渲染（懒加载）
- 验证 396 篇文章的加载时间

## Migration Plan

### Phase 1: 基础设施准备（不影响用户）

1. 创建 `knowledge_hub/` 目录结构
2. 复制现有代码到新目录
3. 执行所有文件名下划线化
4. 更新所有 import/require 引用
5. 更新所有路由配置

### Phase 2: 图解系列 MD 迁移

1. 编写迁移脚本 `convert_to_md.ts`
2. 生成所有 `.md` 文件
3. 创建新的 `learn_deep_index.ts`
4. 抽检验证格式正确性

### Phase 3: Markdown 渲染组件

1. **H5 端**：实现 `MarkdownRenderer.tsx`
2. **小程序端**：实现兼容版解析器
3. 集成到 `learn_deep` 页面

### Phase 4: 目录重命名（Git 操作）

```bash
# 最后一步
git mv reading-radar-2 knowledge_hub
```

### Phase 5: 验证

```bash
# 验证清单
bunx tsc --noEmit                    # TypeScript 编译
node web/scripts/test-pages.cjs       # H5 冒烟
bun run build:h5                      # H5 构建
bun run build:weapp                   # 小程序构建
```

### Rollback 策略

如遇重大问题，回滚到 `reading-radar-2/` 分支：
```bash
git checkout reading-radar-2
```

## Open Questions

1. **E2E 测试**：重命名后 `e2e/*.spec.ts` 中的选择器路径是否需要同步更新？
   - 预计需要更新（基于组件类名的选择器）

2. **BUTTONS.md 同步**：按钮选择器路径变更后，是否需要同步更新？
   - 是，需要在任务中标注

3. **小程序端 Markdown 渲染库**：是否有现成的 Taro 兼容库推荐？
   - 待调研：`taro-markdown` / `taroify` / 自实现
