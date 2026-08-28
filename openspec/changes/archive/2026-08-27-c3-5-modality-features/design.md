# C3-5 各模态功能面补齐设计文档

## 设计目标

聚合各模态纯功能补齐项（不涉及正确性修复）：索引种类、算法库、函数、API、宽表抽象。

## 汇总清单（按模态）

**Relational**：B+Tree 页式化、Hash 索引、BRIN 索引、TOAST、FSM、Extended Query
**Graph**：Cypher 变长路径/OPTIONAL/UNWIND、属性 B-Tree 二级索引、MODEL_RDF 注册
**Timeseries**：标签倒排 + 基数保护、窗口级增量聚合刷新
**Document**：B-Tree 二级索引 + TTL 索引、$lookup/$unwind/$facet/$bucket
**Spatial**：R*Tree 分裂、Multi* 几何类型
**KV**：kv_cas/kv_txn/kv_comparator、跨 CF WriteBatch、宽表第一期
**Tree**：ltree 兼容层

## 依赖

需 C0-C2 全部完成。每个子项独立可验收。
