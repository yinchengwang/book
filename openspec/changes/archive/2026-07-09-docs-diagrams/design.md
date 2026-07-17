# Design: 文档图表体系实施设计

## Context

### 背景

本项目是一个 C/C++ 算法与数据结构学习项目，包含：
- **Phase 1-5**: 基础数据结构与算法
- **Phase 6-8**: 高级特性（向量索引、多模态存储）
- **Phase 9**: 分布式演进（分片路由、Raft 共识、2PC 事务、节点协调）

目前缺乏系统性的图表来帮助学习和教学。图表是理解分布式系统、存储引擎等复杂概念的关键工具。

### 当前状态

- 已有 `docs/storage-architecture.md` 等架构文档，包含部分 ASCII 图表
- 缺乏 Excalidraw 手绘风格的系统性图表
- 无统一的图表文件组织规范

### 约束

- 使用 Excalidraw 手绘风格（`.excalidraw.json` 格式）
- 图表需支持在 VSCode 中直接编辑（Excalidraw 插件）
- 纯文档资产，无代码依赖

## Goals / Non-Goals

**Goals:**
- 建立完整的图表文件组织结构
- 定义 Excalidraw 图表的视觉风格规范（配色、字体、线条）
- 按优先级实现各级别图表
- 为每个图表提供详细的绘制指南

**Non-Goals:**
- 不实现自动化图表生成工具
- 不生成 SVG/PNG 等静态图片文件（由用户按需导出）
- 不绑定特定版本的 Excalidraw

## Decisions

### Decision 1: 目录组织结构

采用层级化的目录结构，按图表所属模块分组：

```
docs/diagrams/
├── level0-project/           # Level 0: 项目级图表
│   ├── L0-001-project-overview.excalidraw.json
│   ├── L0-002-module-dependency.excalidraw.json
│   └── L0-003-learning-path.excalidraw.json
├── level1-distributed/       # Level 1: Phase 9 分布式模块
│   ├── L1-001-distributed-overview.excalidraw.json
│   ├── L1-002-data-structures.excalidraw.json
│   ├── L1-003-shard-routing-flow.excalidraw.json
│   ├── L1-004-raft-state-machine.excalidraw.json
│   ├── L1-005-raft-replication-seq.excalidraw.json
│   ├── L1-006-raft-election-seq.excalidraw.json
│   ├── L1-007-2pc-transaction-flow.excalidraw.json
│   ├── L1-008-2pc-recovery-flow.excalidraw.json
│   ├── L1-009-coordinator-arch.excalidraw.json
│   └── L1-010-coordinator-seq.excalidraw.json
├── level2-storage/           # Level 2: 存储引擎
├── level3-vector/            # Level 3: 向量索引
├── level4-redis-algo/        # Level 4: Redis 与算法
└── _sketches/               # 草图/灵感记录
```

**替代方案考虑:**
- 按图表类型组织（流程图、架构图、时序图）→ 拒绝：不便于按模块查找
- 扁平化结构 → 拒绝：文件多时难以管理

### Decision 2: 文件命名规范

采用 `{Level}-{序号}-{简短描述}.excalidraw.json` 格式：
- `Level` 标识图表所属层级
- `序号` 从 001 开始，保证排序正确
- `描述` 使用 kebab-case，简短概括图表内容

### Decision 3: Excalidraw 视觉风格

定义统一的视觉风格，保证所有图表风格一致：

| 元素 | 规范 |
|------|------|
| 线条粗细 | 2px |
| 线条颜色 | #1a1a1a (深灰) |
| 背景色块 | #fff9db (浅黄) / #e7f5ff (浅蓝) |
| 核心/主流程 | #1971c7 (深蓝) |
| 成功/完成 | #2f9e44 (绿色) |
| 重点/强调 | #e03131 (红色) |
| 字体 | Excalidraw 内置手写字体 |

### Decision 4: 优先级定义

按实施优先级分为三层：

| 优先级 | 说明 | 涵盖图表 |
|--------|------|----------|
| P0 | 核心必做，面试高频 | L0-001, L1-003, L1-004, L1-007, L2-002 |
| P1 | 重要支撑，模块全景 | L1-001, L1-002, L1-009, L3-001, L4-006 |
| P2 | 完善补充 | 其余图表 |

### Decision 5: 图表内容格式

每个 Excalidraw 文件包含：
- 手绘风格的可视化图形
- 关键元素用文字标注
- 图表底部包含绘制说明（Excalidraw 的 Text 元素）

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| Excalidraw 插件更新导致格式兼容问题 | 低 | 保持关注插件版本更新 |
| 图表数量多，维护成本增加 | 中 | 按层级分目录组织，提供绘制指南 |
| 多人协作可能产生冲突 | 低 | 每个图表独立文件，减少冲突 |

## Open Questions

1. 是否需要创建图表的 SVG 导出版本存放在 `docs/diagrams/generated/`？
2. 是否需要在 README.md 中添加图表索引链接？
3. 是否需要为每个图表编写对应的 Markdown 说明文档？
