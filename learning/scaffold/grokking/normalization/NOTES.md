# 范式 · 学习笔记

## 核心概念

1. **1NF（第一范式）**：列不可再分，无重复组 —— 每个字段是原子的，表中不能有"列表列"
2. **2NF（第二范式）**：满足 1NF + 非主属性完全依赖于主键 —— 消除部分函数依赖
3. **3NF（第三范式）**：满足 2NF + 非主属性不传递依赖于主键
4. **BCNF**：每个决定因子（左侧）都必须是候选键
5. **反规范化**：为查询性能有意引入冗余，写入时多维护、读取时免 JOIN

## 工程对照

数据库设计是数据库内核的上层约束。本项目中 `engineering/src/db/catalog/` 的
`catalog.c` 维护系统表 `pg_class`（表元信息）和 `pg_attribute`（列元信息），
每张表的列定义、类型和约束都在这些系统表中记录。表设计的范式化体现在 SQL DDL
的约束声明中（PRIMARY KEY / FOREIGN KEY / UNIQUE），而存储引擎本身不强制范式，
它将数据按页面组织存储，查询时通过 Join 算子（`engineering/src/db/sql/sql_exec.c`
中的 NestLoopJoin / HashJoin）实现表间关联。
