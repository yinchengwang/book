# SQL 基础 · 学习笔记

## 核心概念

1. **SELECT 执行顺序**：FROM → WHERE → GROUP BY → HAVING → SELECT → ORDER BY → LIMIT
2. **JOIN 类型**：INNER JOIN（匹配交集）、LEFT JOIN（左表全留）、CROSS JOIN（笛卡尔积）
3. **聚合函数**：COUNT/SUM/AVG/MIN/MAX 忽略 NULL；GROUP BY 分组后聚合
4. **索引**：BTree 索引加速等值/范围查询，复合索引遵守最左前缀原则

## 工程对照

本项目的 `engineering/src/db/sql/` 目录实现了完整的 SQL 解析流程：
`sql_parser.y` 使用 Lemon 解析器生成器将 SQL 文本解析为 AST（语法树），
`sql_exec.c` 遍历 AST 节点完成 DDL（CREATE TABLE）、DML（SELECT/INSERT/UPDATE/DELETE）
的执行。其中 SELECT 的执行与本文演示类似：先 FROM 定位表、JOIN 生成中间结果、
WHERE 过滤、GROUP BY 分组、聚合计算、HAVING 二次过滤。差异在于工程实现采用
火山模型（Volcano Iterator Model），每个算子通过 `next()` 接口逐行拉取数据，
而本文用 SQLite 库函数直接封装了完整执行过程。
