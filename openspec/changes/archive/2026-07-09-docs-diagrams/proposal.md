# Proposal: 文档图表体系建设

## Why

本项目包含大量复杂的数据结构和算法实现（尤其是 Phase 9 分布式模块），目前缺乏系统性的图表来帮助学习和教学。图表是理解分布式系统、存储引擎等复杂概念的关键工具，既服务于个人复习巩固，也支持向他人讲解和分享。

## What Changes

- 创建 `docs/diagrams/` 目录，按层级组织图表文件
- 使用 Excalidraw 手绘风格创建所有图表（`.excalidraw.json` 格式）
- 按四个层级实现图表：
  - **Level 0**: 项目全景图（全景地图、模块依赖、学习路线）
  - **Level 1**: Phase 9 分布式模块（分片路由、Raft 状态机、2PC 事务、节点协调器）
  - **Level 2**: 存储引擎（Buffer Pool、Heap/BTree Page、WAL、检查点）
  - **Level 3**: 向量索引与多模态（HNSW/IVF-PQ/DiskANN、多模态路由）
  - **Level 4**: Redis 与算法（数据结构全景、排序对比）
- 按优先级分批实施：P0（核心必做）→ P1（重要）→ P2（完善）
- 为每个图表提供绘制指南（文本描述），便于后续维护和扩展

## Capabilities

### New Capabilities

- `docs-diagrams-project`: 项目全景图表
  - 项目全景地图、模块依赖关系图、学习路线图、技术栈概览

- `docs-diagrams-distributed`: Phase 9 分布式模块图表
  - 分布式模块全景图、核心数据结构关系图
  - 分片路由流程图、Raft 状态机图、Raft 日志复制/选举时序图
  - 2PC 事务流程图、2PC 故障恢复流程图
  - 节点协调器架构图、节点协调流程时序图
  - 分片拓扑管理图、一致性 Hash 环图、动态扩缩容流程图

- `docs-diagrams-storage`: 数据库存储引擎图表
  - 存储引擎全景图、Buffer Pool 置换流程图
  - 页面生命周期图、Heap Page 结构图、BTree 页面分裂流程图
  - WAL 日志格式图、检查点流程图、Catalog 系统结构图
  - 元组操作流程图、SQL 执行流程图

- `docs-diagrams-vector`: 向量索引与多模态存储图表
  - 向量索引选择决策树、HNSW 层结构/搜索流程图
  - IVF+PQ 索引结构图、DiskANN 架构图、向量索引对比表
  - 多模态引擎路由图、跨模型联合查询流程图
  - 时序引擎聚合查询图、空间索引 R-Tree 图

- `docs-diagrams-redis-algo`: Redis 与算法模块图表
  - Redis 对象系统图、SDS 数据结构图、跳表结构图
  - Redis 持久化流程图、算法分类全景图
  - 排序算法对比表、数据结构选择决策树
  - KMP 匹配流程图、K-Means 聚类流程图、量化压缩原理图

### Modified Capabilities

（无）

## Impact

- **新增目录**: `docs/diagrams/` 及子目录
- **新增文件**: 约 40 个 Excalidraw 图表文件（`.excalidraw.json`）
- **依赖关系**: 无代码依赖，纯文档资产
- **使用方式**: 可直接在 VSCode 中用 Excalidraw 插件打开编辑，或导出为 SVG/PNG 用于文档和演示
