# sql-executor Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: Volcano 迭代器模型

系统 SHALL 实现 Volcano 迭代器模型的查询执行引擎。

#### Scenario: 迭代器生命周期
- **WHEN** 执行物理计划
- **THEN** 每个算子 SHALL 实现 Init() → Exec() → End() 三阶段
- **THEN** Exec() SHALL 返回下一行数据或 NULL（迭代结束）

#### Scenario: 算子组合
- **WHEN** 执行复合计划（如 Scan → Filter → Project）
- **THEN** 下游算子 SHALL 调用上游算子的 Exec()
- **THEN** 数据 SHALL 按需从下往上拉取

---

### Requirement: Scan 算子实现

系统 SHALL 实现各种扫描算子。

#### Scenario: SeqScan 实现
- **WHEN** 执行 SeqScan
- **THEN** 算子 SHALL 顺序扫描指定表
- **THEN** 过滤条件 SHALL 被应用
- **THEN** 匹配行 SHALL 被返回

#### Scenario: IndexScan 实现
- **WHEN** 执行 IndexScan
- **THEN** 算子 SHALL 使用索引定位数据
- **THEN** 索引条件 SHALL 先被应用
- **THEN** 回表获取完整行数据（如需要）

#### Scenario: VectorScan 实现
- **WHEN** 执行 VectorScan
- **THEN** 算子 SHALL 调用向量引擎进行相似度搜索
- **THEN** 结果 SHALL 转换为关系行格式
- **THEN** 属性过滤 SHALL 被组合

#### Scenario: TimeSeriesScan 实现
- **WHEN** 执行 TimeSeriesScan
- **THEN** 算子 SHALL 使用时间索引定位数据
- **THEN** 时间范围 SHALL 被正确过滤
- **THEN** 聚合 SHALL 在扫描时应用（如配置）

---

### Requirement: Join 算子实现

系统 SHALL 实现各种连接算子。

#### Scenario: HashJoin 实现
- **WHEN** 执行 HashJoin
- **THEN** 构建阶段 SHALL 构建哈希表
- **THEN** 探测阶段 SHALL 逐行探测
- **THEN** 连接条件 SHALL 被正确判断

#### Scenario: NestedLoopJoin 实现
- **WHEN** 执行 NestedLoopJoin
- **THEN** 外表 SHALL 被顺序扫描
- **THEN** 内表 SHALL 对每行外表进行扫描
- **THEN** 连接条件 SHALL 被检查

#### Scenario: MergeJoin 实现
- **WHEN** 执行 MergeJoin
- **THEN** 两表 SHALL 按连接键排序
- **THEN** 归并算法 SHALL 被应用

---

### Requirement: Aggregation 算子实现

系统 SHALL 实现聚合算子。

#### Scenario: HashAgg 实现
- **WHEN** 执行 HashAgg
- **THEN** 分组键 SHALL 被哈希
- **THEN** 相同键的值 SHALL 被聚合
- **THEN** 聚合结果 SHALL 被输出

#### Scenario: SortAgg 实现
- **WHEN** 执行 SortAgg
- **THEN** 输入 SHALL 按分组键排序
- **THEN** 相同键的值 SHALL 被连续处理
- **THEN** 减少内存使用

#### Scenario: 窗口函数实现
- **WHEN** 执行包含窗口函数的查询
- **THEN** 窗口函数 SHALL 正确计算
- **THEN** PARTITION BY 和 ORDER BY SHALL 被支持

---

### Requirement: Modification 算子实现

系统 SHALL 实现数据修改算子。

#### Scenario: Insert 实现
- **WHEN** 执行 INSERT
- **THEN** 插入算子 SHALL 调用存储引擎插入数据
- **THEN** 自增主键 SHALL 被正确处理
- **THEN** 影响行数 SHALL 被返回

#### Scenario: Update 实现
- **WHEN** 执行 UPDATE
- **THEN** 更新算子 SHALL 定位并修改数据
- **THEN** WHERE 条件 SHALL 被应用
- **THEN** 影响行数 SHALL 被返回

#### Scenario: Delete 实现
- **WHEN** 执行 DELETE
- **THEN** 删除算子 SHALL 定位并删除数据
- **THEN** WHERE 条件 SHALL 被应用
- **THEN** 影响行数 SHALL 被返回

---

### Requirement: 表达式求值

系统 SHALL 实现表达式求值引擎。

#### Scenario: 算术表达式求值
- **WHEN** 求值算术表达式
- **THEN** +、-、*、/ SHALL 被正确计算
- **THEN** 类型转换 SHALL 被自动处理

#### Scenario: 逻辑表达式求值
- **WHEN** 求值逻辑表达式
- **THEN** AND、OR、NOT SHALL 被正确计算
- **THEN** 短路求值 SHALL 被应用

#### Scenario: 函数调用求值
- **WHEN** 求值函数调用
- **THEN** 内置函数 SHALL 被正确执行
- **THEN** 用户定义函数 SHALL 被调用

#### Scenario: NULL 值处理
- **WHEN** 表达式包含 NULL
- **THEN** NULL 算术运算 SHALL 返回 NULL
- **THEN** NULL 比较 SHALL 按三值逻辑处理
- **THEN** COALESCE SHALL 正确返回非 NULL 值

---

### Requirement: 结果集管理

系统 SHALL 实现结果集管理。

#### Scenario: 结果缓冲
- **WHEN** 执行查询
- **THEN** 结果 SHALL 被缓冲以提高性能
- **THEN** 缓冲区大小 SHALL 可配置

#### Scenario: 结果返回
- **WHEN** 查询执行完成
- **THEN** 结果集 SHALL 以标准格式返回
- **THEN** 元数据（列名、类型）SHALL 被包含

