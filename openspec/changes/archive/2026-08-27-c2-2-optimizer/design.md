# C2-2 优化器实现 设计文档

## 设计目标

替换 02 卷识别的 TODO 桩：5 条优化规则全部原样返回；选择率仅支持等值；摘掉"已优化"假开关。

## 方案

### 1. 摘假开关（T1）

`optimizer.c:83-84` 的 `enable_join_order` 等配置默认 true → 未实现则改为 false。同时 LOG_WARN 提示"实现待补"。

### 2. AttStats 扩展（T2）

`AttStats` 扩展字段：
- `int32_t *histogram_bounds`（等深 100 桶边界）
- `AttStatsMCV *mcv`（最常见值列表，max 100）
- `double correlation`（物理位置与值的相关性）

catalog 持久化：page 元数据存 stats blob（序列化 AttStats）。

### 3. ANALYZE 命令（T3）

解析层接受 `ANALYZE [table_name]`：
- 若指定表：对表采样（默认 30000 行）
- 计算 ndistinct / histogram / MCV
- 持久化到 catalog

### 4. 选择率函数（T4）

`eqsel`（等值）：`min(1/ndistinct, mcv_freq)`
`rangesel`（范围）：线性插值于 histogram
`joinsel`（连接）：`1 / max(ndistinct_a, ndistinct_b)`

### 5. join DP（T5）

≤10 表精确枚举（自底向上 DP）：
- 集合 R 表示已 join 的表
- 代价 = Σ 单表代价 + Σ 连接代价
- 相邻集合按可用连接谓词连接

### 6. EXPLAIN（T6）

`EXPLAIN SELECT ...` 输出：
- 计划类型（SeqScan/IndexScan/HashJoin/NL）
- 估算行数
- 代价（cost）