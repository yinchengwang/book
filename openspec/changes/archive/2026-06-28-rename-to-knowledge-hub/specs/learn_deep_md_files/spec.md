# 图解系列 MD 文件迁移规范

## Purpose

将图解系列的 396 篇文章从 `learn-deep-content.ts` 迁移到独立的 `.md` 文件，符合 CLAUDE.md 规定的"所有内容数据必须以 Markdown 文件存储"的规范。

## ADDED Requirements

### Requirement: MD 文件目录结构

图解系列 MD 文件 SHALL 按以下目录结构组织：

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
├── redis/
│   └── ...
├── database/
│   └── ...
├── linux/
│   └── ...
└── ...
```

#### Scenario: 文件命名规范

- **WHEN** 迁移标题为"数据类型与运算符"、分类为"C"的文章
- **THEN** 生成文件名为 `src/data/learn_deep/c/c_syntax.md`（slug: `c_syntax`）

#### Scenario: 同一分类下 slug 唯一

- **WHEN** 迁移时发现同分类下 slug 冲突
- **THEN** 在 slug 后追加数字后缀（如 `c_syntax_2.md`）

### Requirement: MD 文件格式规范

每个 MD 文件 SHALL 包含 YAML frontmatter 元数据：

```markdown
---
title: 数据类型与运算符
category: C
slug: c_syntax
excerpt: C语言的数据类型规定了变量在内存中占用的字节数...
---

正文内容...
```

#### Scenario: 正确解析 frontmatter

- **WHEN** 渲染 `src/data/learn_deep/c/c_syntax.md`
- **THENEN** frontmatter 中的 title、category、excerpt 被正确提取用于列表展示

#### Scenario: 无 frontmatter 降级

- **WHEN** MD 文件缺少 frontmatter
- **THENEN** 从文件名和内容首行推断元数据，不阻断渲染

### Requirement: 内容迁移完整性

迁移 SHALL 保证所有原有内容完整迁移，不丢失任何信息。

#### Scenario: 代码块保留

- **WHEN** 原文包含 ` ```c\nint x = 1;\n``` `
- **THEN** MD 文件中完整保留代码块语法

#### Scenario: 图片链接保留

- **WHEN** 原文包含 `![内存模型](https://example.com/img.png)`
- **THEN** MD 文件中完整保留图片语法

#### Scenario: 内部链接处理

- **WHEN** 原文包含内部链接 `[函数与作用域](c-function-scope)`
- **THEN** MD 文件中链接被转换为基于 slug 的链接格式

### Requirement: 索引文件生成

迁移后 SHALL 生成新的索引文件 `learn_deep_index.ts`，仅包含元数据：

```typescript
export interface LearnDeepSummary {
  slug: string
  title: string
  category: string
  excerpt: string
  length: number
}

export const learn_deep_list: LearnDeepSummary[] = [
  { slug: 'c_syntax', title: '数据类型与运算符', category: 'C', ... }
]
```

#### Scenario: 索引包含所有文章

- **WHEN** 加载 `learn_deep_index.ts`
- **THEN** 索引数组长度为 396，包含所有文章元数据

#### Scenario: 按分类查询

- **WHEN** 查询分类为"C"的所有文章
- **THEN** 返回该分类下的所有文章元数据

### Requirement: 内容按需加载

内容 SHALL 按需从 MD 文件加载，而非全部预加载。

#### Scenario: 懒加载文章内容

- **WHEN** 用户打开某篇文章详情页
- **THEN** 仅加载该文章的 MD 文件内容

#### Scenario: 批量加载优化

- **WHEN** 用户浏览列表页
- **THEN** 仅加载索引文件，不加载任何 MD 文件正文
