# Spec: docs-diagrams-redis-algo

## ADDED Requirements

### Requirement: Redis 与算法模块图表文件结构

`docs/diagrams/level4-redis-algo/` 目录 SHALL 包含以下 Excalidraw 图表文件：

- `L4-001-redis-object-system.excalidraw.json` - Redis 对象系统图
- `L4-002-sds-structure.excalidraw.json` - SDS 数据结构图
- `L4-003-skiplist-structure.excalidraw.json` - 跳表结构图
- `L4-004-redis-persistence-flow.excalidraw.json` - Redis 持久化流程图
- `L4-005-algo-taxonomy.excalidraw.json` - 算法分类全景图
- `L4-006-sorting-comparison.excalidraw.json` - 排序算法对比表
- `L4-007-ds-selection-tree.excalidraw.json` - 数据结构选择决策树
- `L4-008-kmp-flow.excalidraw.json` - KMP 匹配流程图
- `L4-009-kmeans-flow.excalidraw.json` - K-Means 聚类流程图
- `L4-010-quantization.excalidraw.json` - 量化压缩原理图

#### Scenario: 目录结构完整性检查

- **WHEN** 用户在 `docs/diagrams/level4-redis-algo/` 目录下列出所有文件
- **THEN** 应当包含上述十个 `.excalidraw.json` 文件

### Requirement: Redis 对象系统图内容

`L4-001-redis-object-system.excalidraw.json` SHALL 包含：

- Redis 五种数据类型：String、List、Hash、Set、ZSet
- 每种类型对应的底层数据结构
- 编码转换规则（如 ZSet 在元素少时用压缩列表，多时用跳表）

#### Scenario: Redis 对象系统完整性

- **WHEN** 用户查看 Redis 对象系统图
- **THEN** 可以理解数据类型和底层实现的对应关系

### Requirement: SDS 数据结构图内容

`L4-002-sds-structure.excalidraw.json` SHALL 包含：

- sdshdr 结构：len（已用长度）、free（剩余长度）、buf（数据）
- 空间预分配策略
- 二进制安全特性

#### Scenario: SDS 结构清晰性

- **WHEN** 用户查看 SDS 数据结构图
- **THEN** 可以理解 SDS 相比 C 字符串的优势

### Requirement: 跳表结构图内容

`L4-003-skiplist-structure.excalidraw.json` SHALL 包含：

- 多层链表结构
- Level 0 为底层全连接
- 高层节点稀疏
- 与字典（Hash）的双重索引关系
- 随机层高生成算法

#### Scenario: 跳表结构清晰性

- **WHEN** 用户查看跳表结构图
- **THEN** 可以理解跳表的有序性和 O(log n) 查找原理

### Requirement: Redis 持久化流程图内容

`L4-004-redis-persistence-flow.excalidraw.json` SHALL 包含：

- RDB 快照流程：fork → 遍历内存 → 写入 .rdb
- AOF 日志流程：命令追加 → 缓冲区 → 刷盘
- AOF 重写流程：fork → 写 RDB 格式 → 合并增量
- 混合持久化（4.0+）

#### Scenario: 持久化流程完整性

- **WHEN** 用户查看 Redis 持久化流程图
- **THEN** 可以理解 RDB、AOF、混合持久化的区别

### Requirement: 算法分类全景图内容

`L4-005-algo-taxonomy.excalidraw.json` SHALL 包含：

- 基础算法：排序、二分、双指针、滑窗、单调栈
- 高级算法：DP、回溯、分治、贪心、KMP
- 专用算法：K-Means、量化、距离计算、分词

#### Scenario: 算法分类完整性

- **WHEN** 用户查看算法分类全景图
- **THEN** 可以了解本项目的算法覆盖范围

### Requirement: 排序算法对比表内容

`L4-006-sorting-comparison.excalidraw.json` SHALL 包含对比维度：

- 时间复杂度：最好、最坏、平均
- 空间复杂度
- 稳定性
- 算法：冒泡、插入、选择、归并、快速、堆、计数、桶、基数

#### Scenario: 排序对比完整性

- **WHEN** 用户查看排序算法对比表
- **THEN** 可以快速对比各排序算法的特点

### Requirement: 数据结构选择决策树内容

`L4-007-ds-selection-tree.excalidraw.json` SHALL 包含决策分支：

- 需要快速查找 → 哈希表
- 需要有序遍历 → 平衡树/跳表
- 需要快速插入删除 → 链表
- 需要范围统计 → 线段树/树状数组

#### Scenario: 数据结构选择正确性

- **WHEN** 用户根据需求选择数据结构
- **THEN** 可以通过决策树快速定位合适的数据结构

### Requirement: KMP 匹配流程图内容

`L4-008-kmp-flow.excalidraw.json` SHALL 包含：

- 部分匹配表（PMT/prefix function）的构建过程
- 匹配失败时的跳转规则
- 主串和模式串的匹配过程

#### Scenario: KMP 流程完整性

- **WHEN** 用户查看 KMP 匹配流程图
- **THEN** 可以理解 KMP 如何避免暴力匹配的回溯

### Requirement: K-Means 聚类流程图内容

`L4-009-kmeans-flow.excalidraw.json` SHALL 包含：

- 初始化：随机选择 K 个中心点
- 分配：将点分配到最近的中心
- 更新：重新计算中心点位置
- 迭代：重复直到收敛

#### Scenario: K-Means 流程完整性

- **WHEN** 用户查看 K-Means 聚类流程图
- **THEN** 可以理解聚类的迭代过程

### Requirement: 量化压缩原理图内容

`L4-010-quantization.excalidraw.json` SHALL 包含：

- PQ (Product Quantization)：向量分块 → K-Means 聚类 → 编码
- LVQ (Learning Vector Quantization)：学习码本
- 原始向量 vs 量化后向量的存储对比
- SIMD 距离计算加速

#### Scenario: 量化压缩原理清晰性

- **WHEN** 用户查看量化压缩原理图
- **THEN** 可以理解向量量化压缩的原理
