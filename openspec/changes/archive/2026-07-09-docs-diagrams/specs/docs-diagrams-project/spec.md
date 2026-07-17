# Spec: docs-diagrams-project

## ADDED Requirements

### Requirement: 项目全景图表文件结构

`docs/diagrams/level0-project/` 目录 SHALL 包含以下 Excalidraw 图表文件：

- `L0-001-project-overview.excalidraw.json` - 项目全景地图
- `L0-002-module-dependency.excalidraw.json` - 模块依赖关系图
- `L0-003-learning-path.excalidraw.json` - 学习路线图

#### Scenario: 目录结构完整性检查

- **WHEN** 用户在 `docs/diagrams/level0-project/` 目录下列出所有文件
- **THEN** 应当包含上述三个 `.excalidraw.json` 文件

### Requirement: 项目全景地图内容

`L0-001-project-overview.excalidraw.json` SHALL 包含以下内容：

- 项目的三层架构：基础层（self_made/algo/leetcode）、索引层（index/）、存储引擎层（db/）
- Phase 9 分布式模块的突出展示
- 学习路径指引（Phase 1-5 → Phase 6-8 → Phase 9）

#### Scenario: 全景地图覆盖范围

- **WHEN** 用户打开项目全景地图
- **THEN** 可以看到 self_made、algo、leetcode、index、db 五个主要模块
- **AND** 可以看到各模块之间的依赖关系
- **AND** 可以看到分布式模块（Phase 9）的位置

### Requirement: 模块依赖关系图内容

`L0-002-module-dependency.excalidraw.json` SHALL 包含以下内容：

- src/ 与 include/ 的编译依赖关系
- 各库之间的依赖层次
- 外部参考源码（open_source/）的位置

#### Scenario: 依赖关系正确性

- **WHEN** 用户查看模块依赖关系图
- **THEN** 可以清晰看到 index → algo → project_includes 的依赖链
- **AND** 可以看到 db/storage → db/catalog → db/lock 的依赖链

### Requirement: 学习路线图内容

`L0-003-learning-path.excalidraw.json` SHALL 包含以下内容：

- Phase 1-5: 基础阶段（数据结构、基础算法）
- Phase 6-8: 高级特性（向量索引、多模态）
- Phase 9: 分布式演进（分片、Raft、2PC）

#### Scenario: 学习路径连贯性

- **WHEN** 用户参考学习路线图规划学习计划
- **THEN** 可以按照 Phase 顺序逐步深入
- **AND** 可以了解每个阶段的核心知识点
