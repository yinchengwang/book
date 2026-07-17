# Spec: docs-diagrams-storage

## ADDED Requirements

### Requirement: 数据库存储引擎图表文件结构

`docs/diagrams/level2-storage/` 目录 SHALL 包含以下 Excalidraw 图表文件：

- `L2-001-storage-overview.excalidraw.json` - 存储引擎全景图
- `L2-002-buffer-pool-flow.excalidraw.json` - Buffer Pool 置换流程图
- `L2-003-page-lifecycle.excalidraw.json` - 页面生命周期图
- `L2-004-heap-page-structure.excalidraw.json` - Heap Page 结构图
- `L2-005-btree-split-flow.excalidraw.json` - BTree 页面分裂流程图
- `L2-006-wal-format.excalidraw.json` - WAL 日志格式图
- `L2-007-checkpoint-flow.excalidraw.json` - 检查点流程图
- `L2-008-catalog-structure.excalidraw.json` - Catalog 系统结构图
- `L2-009-tuple-operations.excalidraw.json` - 元组操作流程图
- `L2-010-sql-execution-flow.excalidraw.json` - SQL 执行流程图

#### Scenario: 目录结构完整性检查

- **WHEN** 用户在 `docs/diagrams/level2-storage/` 目录下列出所有文件
- **THEN** 应当包含上述十个 `.excalidraw.json` 文件

### Requirement: 存储引擎全景图内容

`L2-001-storage-overview.excalidraw.json` SHALL 包含以下层次结构：

- SQL 执行器层（Parser、Analyzer、Optimizer、Executor）
- Access Method 层（Heap AM、BTree AM、GiST、GIN）
- Buffer Pool 层（Hash 表、Clock-Sweep、缓冲区、描述符）
- Catalog 层（pg_class、pg_attribute、pg_index）
- WAL 层（Redo Log、Checkpoint、Buffer 协调）
- 磁盘文件层（page.c、disk.c、tablespace）

#### Scenario: 存储引擎层次完整性

- **WHEN** 用户打开存储引擎全景图
- **THEN** 可以看到从 SQL 执行到磁盘文件的完整层次

### Requirement: Buffer Pool 置换流程图内容

`L2-002-buffer-pool-flow.excalidraw.json` SHALL 展示完整的页面查找和置换流程：

- Hash 表命中检查
- 未命中时执行 Clock-Sweep 寻找可置换页面
- 脏页判断：需要刷盘
- 读取新页面到缓冲区
- 返回页面引用

#### Scenario: Buffer Pool 置换流程完整性

- **WHEN** 用户查看 Buffer Pool 置换流程图
- **THEN** 可以清晰看到页面查找、未命中、置换、刷盘的完整流程

### Requirement: 页面生命周期图内容

`L2-003-page-lifecycle.excalidraw.json` SHALL 包含：

- 磁盘 → Buffer（读取）
- Buffer 修改 → 标记脏页
- 脏页 → 刷盘 → 磁盘
- Pin/Unpin 引用计数管理
- Buffer 淘汰

#### Scenario: 页面生命周期完整性

- **WHEN** 用户查看页面生命周期图
- **THEN** 可以理解页面从磁盘到内存再到磁盘的完整流转

### Requirement: Heap Page 结构图内容

`L2-004-heap-page-structure.excalidraw.json` SHALL 包含：

- Page Header（页面头部）
- ItemIdData（行指针数组）
- Free Space（空闲空间）
- Tuple Data（实际数据）
- Special Space（特殊空间）

#### Scenario: Heap Page 结构清晰性

- **WHEN** 用户查看 Heap Page 结构图
- **THEN** 可以理解页面的内部布局和各区域用途

### Requirement: BTree 页面分裂流程图内容

`L2-005-btree-split-flow.excalidraw.json` SHALL 包含：

- 插入导致页面满
- 分配新页面
- 分裂数据（移动前半部分到新页）
- 更新父节点指向新页
- 可能触发父节点级联分裂

#### Scenario: BTree 分裂流程完整性

- **WHEN** 用户查看 BTree 页面分裂流程图
- **THEN** 可以理解页面分裂的完整过程

### Requirement: WAL 日志格式图内容

`L2-006-wal-format.excalidraw.json` SHALL 包含：

- WAL Record 结构（LSN、TransactionId、Prev LSN、Length、Type、Page ID、Data）
- 日志写入顺序
- 检查点记录格式

#### Scenario: WAL 日志格式完整性

- **WHEN** 用户查看 WAL 日志格式图
- **THEN** 可以理解 WAL Record 的内部结构

### Requirement: 检查点流程图内容

`L2-007-checkpoint-flow.excalidraw.json` SHALL 包含：

- 检查点触发条件（时间间隔、事务数、WAL 段数）
- 遍历所有脏页
- 刷盘所有脏数据页
- 写入检查点记录
- 更新控制文件

#### Scenario: 检查点流程完整性

- **WHEN** 用户查看检查点流程图
- **THEN** 可以理解检查点的完整执行过程

### Requirement: Catalog 系统结构图内容

`L2-008-catalog-structure.excalidraw.json` SHALL 包含：

- pg_class 表（表名、OID、relkind、relfilenode）
- pg_attribute 表（列名、类型、位置、默认值）
- pg_index 表（索引 OID、表 OID、索引列）
- 系统表缓存机制

#### Scenario: Catalog 结构完整性

- **WHEN** 用户查看 Catalog 系统结构图
- **THEN** 可以理解系统表的结构和关系

### Requirement: 元组操作流程图内容

`L2-009-tuple-operations.excalidraw.json` SHALL 包含：

- Insert：获取页面 → 写入元组 → 更新 ItemId → 标记脏页
- Update：定位元组 → 写入新版本 → 标记旧版本无效 → 标记脏页
- Delete：定位元组 → 标记删除标记 → 标记脏页

#### Scenario: 元组操作流程完整性

- **WHEN** 用户查看元组操作流程图
- **THEN** 可以理解 Insert/Update/Delete 的完整流程

### Requirement: SQL 执行流程图内容

`L2-010-sql-execution-flow.excalidraw.json` SHALL 包含：

- Parser：SQL 文本 → AST
- Analyzer：AST → 查询树
- Optimizer：查询树 → 执行计划
- Executor：执行计划 → 结果

#### Scenario: SQL 执行流程完整性

- **WHEN** 用户查看 SQL 执行流程图
- **THEN** 可以理解从 SQL 到结果的完整处理流程
