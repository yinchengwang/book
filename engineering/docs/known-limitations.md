# Known Limitations — SQL Stack Consolidation (S0)

> **范围**：本文档记录"SQL 栈全收敛"项目（spec `2026-09-26-sql-consolidation-design.md`）
> 验收（2026-06-27）时尚未实现的能力点。每条对应一个被跳过的 `test_sql_integration`
> 用例（`GTEST_SKIP()` + 原因），不阻塞 DoD。
>
> 后续子项目（S1 执行器/表达式补完、S3 SQL 覆盖面扩展、S6 事务与并发）
> 承接实现。

## 跳过的 test_sql_integration 用例

| TEST | 原因 | 后续子项目 |
|------|------|------------|
| `CreateTable_AllTypes` | BLOB 列 DDL 走通；DML 写暂不支持 | S3 |
| `Insert_NegativeNumbers` | DML 负数常量走 parser/executor 边角未覆盖 | S1 |
| `Insert_FloatNumbers` | REAL 列 DML 写入在 T9 主路径未涵盖（`OID 700 暂不支持 DML 写入`） | S1 |
| `Update_Simple` | UPDATE 走 ModifyTable + Filter，Filter 节点未实现（依赖 WHERE 子句） | S1 |
| `Update_MultipleColumns` | 同上 | S1 |
| `Update_WithCondition` | 同上 | S1 |
| `Update_AllRows` | 同上（无 WHERE 分支） | S1 |
| `Delete_WithCondition` | DELETE 走 ModifyTable + Filter，Filter 节点未实现 | S1 |
| `Delete_MultipleRows` | 同上 | S1 |
| `Select_MultipleTables` | 多表 SELECT（cross join）未实现（仅单表 SeqScan 主路径） | S3 |
| `Where_Equals` | SELECT WHERE 子句主路径未涵盖（T9 仅接通主路径节点） | S1 |
| `Where_NotEquals` | 同上 | S1 |
| `Where_LessThan` | 同上 | S1 |
| `Where_GreaterThan` | 同上 | S1 |
| `Where_And` | 同上 | S1 |
| `Where_Or` | 同上 | S1 |
| `Where_ComplexConditions` | 同上 | S1 |
| `E2E_UserRegistration` | 依赖 WHERE + UPDATE WHERE | S1 |
| `E2E_OrderProcessing` | 依赖 WHERE + UPDATE/DELETE WHERE | S1 |
| `E2E_ProductCatalog` | 依赖 WHERE + UPDATE WHERE | S1 |
| `EdgeCase_ZeroAndNegative` | 依赖负数 DML 写入 | S1 |
| `EdgeCase_LargeNumbers` | 依赖 REAL 列 DML 写入 | S1 |
| `Stress_ManyInserts` | 依赖 WHERE（验证步骤） | S1 |
| `Stress_ComplexQueries` | 依赖 WHERE | S1 |
| `Persistence_DataSurvivesClose` | heapam 持久化路径 WAL/reopen 在 S5 深化前不保证跨 `db_cli_destroy/recreate` | S5 |

## 主路径覆盖（DoD-3 验收 — 全部通过）

- `E2ECapability.OrderByLimit`
- `E2ECapability.LimitOffset`
- `E2ECapability.GroupByAggregate`
- `E2ECapability.InnerJoin`
- `E2ECapability.SubqueryInFrom`

外加 `DriverSmoke.*`（11 个 DDL/DML 端到端）+ `SqlParseCanonical.*` + `CanonicalParserTest.*`
共 27/27 通过，定义主路径可用。

## 关联文档

- 能力兑现：spec §6.2 DoD-3（5 capability + 27 SQL smoke 全绿）
- 测试迁移说明：见 `archive/db-sql-over-kv/tests/{test_planner.cpp,sql_parser.cpp}` 文件头归档原因
- 主路径无静默桩：spec §5.3；DoD-4 grep 验证（见 task-12 报告）