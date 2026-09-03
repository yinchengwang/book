# 索引设计 · 学习笔记

## 核心概念

1. **BTree 索引**：平衡多路搜索树，适合等值（O(logN)）和范围查询；SQLite/InnoDB 默认索引类型
2. **Hash 索引**：哈希表 O(1) 精确匹配，不支持范围查询；用于 Memory 引擎或自适应 Hash 索引
3. **复合索引**：多列联合索引，遵循最左前缀原则——跳过最左列则无法使用索引
4. **覆盖索引**：索引包含查询所需全部列，无需回表访问数据页，IO 大幅减少
5. **最左前缀原则**：复合索引 (a,b,c) 等同于创建了 (a)、(a,b)、(a,b,c) 三个索引

## 工程对照

本项目的 `engineering/src/db/index/` 实现了 BTree 索引（`btreeam.c`）。
核心结构是 `BTreePage` 和 `BTreeKey`，通过 `btree_insert()` / `btree_search()` /
`btree_scan()` 接口实现索引的增删改查。SQL 层（`sql_exec.c`）在执行 SELECT
时通过优化器（`optimizer.c`）判断是否使用索引：若 WHERE 条件匹配索引键，
则走 IndexScan 而非 SeqScan。
