# OLAP 数据库面试题

> 本文档整理 OLAP 列式存储数据库相关的面试题目，涵盖列式存储原理、向量化执行、物化视图、查询优化器、数据压缩等核心知识点。

---

## 1. 列式存储原理

### Q1: 什么是列式存储？与行式存储相比有什么优势？

**答案：**

列式存储（Column-Oriented Storage）将数据按列而不是按行存储在磁盘上。

**优势：**
- **列裁剪（Column Pruning）**：读取查询需要的列，避免读取无关列
- **高压缩比**：同一列数据类型一致，可使用更高效的编码（字典、位图、行程编码）
- **向量化的列式计算**：相同列的数据可以连续处理，充分利用 CPU 缓存和 SIMD
- **聚合查询快**：COUNT、SUM、AVG 等聚合操作只需读取相关列

**行式存储优势场景：**
- 点查询：根据主键获取整行数据
- 写入密集型场景

### Q2: 列式存储中如何实现数据更新？

**答案：**

列式存储通常采用以下策略处理更新：

1. **追加写（Append-Only）**
   - 新数据追加到新的列文件
   - 避免随机写

2. **Delta Tree 结构**
   - 最近的小批量数据以行存形式存储在内存（MemTable）
   - 定期合并到列存文件

3. **Merge-On-Read**
   - 读取时动态合并历史数据和增量数据
   - 如 ClickHouse 的 Mutation 操作

4. **LSM-Tree 风格**
   - 类似 LevelDB/RocksDB 的分层结构
   - 定期 Compaction 合并数据

### Q3: 什么是列式存储的文件格式？Parquet/ORC 的结构是怎样的？

**答案：**

**Parquet 文件结构：**
```
Parquet File
├── File Header (4 bytes magic number "PAR1")
├── Row Group 1
│   ├── Column Chunk 1 (Col1 stats, data)
│   ├── Column Chunk 2 (Col2 stats, data)
│   └── ...
├── Row Group 2
│   └── ...
├── File Footer
│   ├── Schema
│   ├── Metadata (row groups, columns, stats)
│   └── Magic Number
```

**列存核心概念：**
- **Row Group**：行组，列存内的行块（约 100MB）
- **Column Chunk**：列块，单列在行组中的数据
- **Page**：页，列块的最小读写单元（约 1MB）
- **Stripe**（ORC）：类似 Row Group

---

## 2. 向量化执行

### Q4: 什么是向量化执行（Vectorized Execution）？

**答案：**

向量化执行是一种 CPU 友好的查询执行技术：

1. **批处理（Batch Processing）**
   - 一次处理多个元组（通常 1024 个）
   - 使用 SIMD 指令并行计算

2. **列式布局**
   - 操作符之间传递列式数据块
   - 避免火山模型的逐行迭代开销

3. **Cache-Friendly**
   - 数据在内存中连续排列
   - 提高缓存命中率

**对比火山模型：**
```
火山模型（Tuple-at-a-Time）:
Filter -> Project -> Join -> ...
        ↓          ↓       ↓
     next()     next()   next()  // 每行调用一次

向量化模型（Vectorized）:
Filter -> Project -> Join -> ...
        ↓          ↓       ↓
     1024行    1024行    1024行  // 每次处理一批
```

### Q5: SIMD 在向量化执行中的作用是什么？

**答案：**

SIMD（Single Instruction Multiple Data）允许一条指令同时处理多个数据：

**示例：整数加法**
```c
// 标量版本（4 次循环）
for (i = 0; i < 4; i++) {
    c[i] = a[i] + b[i];
}

// SIMD 版本（1 次指令）
__m256i va = _mm256_loadu_si256(a);  // 加载 8 个 32 位整数
__m256i vb = _mm256_loadu_si256(b);
__m256i vc = _mm256_add_epi32(va, vb); // 同时加 8 个
_mm256_storeu_si256(c, vc);
```

**常见 SIMD 指令集：**
- SSE（128 位）：4 个 float 或 2 个 double
- AVX（256 位）：8 个 float 或 4 个 double
- AVX-512（512 位）：16 个 float 或 8 个 double

**向量化执行中的优化：**
- 过滤（Filter）：`_mm256_cmp_ps` 并行比较
- 哈希表查找：SIMD 批量探测
- 字符串操作：SIMD 模式匹配

---

## 3. 物化视图

### Q6: 什么是物化视图？与普通视图的区别是什么？

**答案：**

| 特性 | 普通视图 | 物化视图 |
|------|---------|---------|
| 数据存储 | 不存储数据，只保存查询定义 | 实际存储查询结果 |
| 查询速度 | 每次执行底层查询 | 直接读取预计算结果 |
| 更新 | 自动反映源表变化 | 需要手动刷新或定时刷新 |
| 存储空间 | 不占空间 | 占用磁盘空间 |
| 适用场景 | 简化复杂查询、隐藏表结构 | 加速聚合查询、报表 |

### Q7: 物化视图的刷新策略有哪些？

**答案：**

1. **全量刷新（Full Refresh）**
   - 删除旧数据，重新执行查询
   - 简单但耗时

2. **增量刷新（Incremental Refresh）**
   - 只刷新变化的数据
   - 依赖变更数据捕获（CDC）

3. **定时刷新（Scheduled Refresh）**
   - 定时任务触发刷新
   - 如 Oracle 的 `REFRESH COMPLETE`

4. **按需刷新（On-Demand Refresh）**
   - 用户手动触发刷新
   - `REFRESH ON DEMAND`

**ClickHouse 的实现：**
- 使用 `MATERIALIZED VIEW`
- 自动增量维护
- 基于触发器或订阅机制

### Q8: 物化视图如何实现实时更新？

**答案：**

**方案一：变更数据捕获（CDC）**
```
源表 → Binlog → Kafka → 物化视图刷新
```

**方案二：Lambda 架构**
- 实时层：最近数据写入内存表
- 批处理层：历史数据定时合并
- 查询时合并两个结果

**方案三：数据库内置机制**
- PostgreSQL：`REFRESH MATERIALIZED VIEW CONCURRENTLY`
- Oracle：使用物化视图日志跟踪变更
- ClickHouse：基于 ClickHouse Keeper 的物化视图

---

## 4. 查询优化器

### Q9: OLAP 查询优化器与 OLTP 优化器有什么不同？

**答案：**

| 方面 | OLTP 优化器 | OLAP 优化器 |
|------|------------|------------|
| **查询特点** | 短查询、点查、主键查找 | 长查询、全表扫描、复杂 JOIN |
| **优化目标** | 低延迟 | 吞吐量、整体执行时间 |
| **优化策略** | 索引选择、简单路径 | 多表 JOIN 重排、并行执行 |
| **统计信息** | 精确行数、直方图 | 近似统计、采样 |
| **执行模式** | 少量数据、随机 IO | 大数据量、顺序 IO |

**OLAP 优化器的特点：**
1. **成本模型**：基于 IO 成本（磁盘扫描成本为主）
2. **JOIN 重排**：使用启发式或动态规划选择最优 JOIN 顺序
3. **并行执行**：支持多线程、分布式执行
4. **向量化友好**：选择支持向量化的算子实现

### Q10: 什么是代价模型（Cost Model）？

**答案：**

代价模型用于评估不同执行计划的成本：

```
Total Cost = Σ (IO Cost + CPU Cost)

IO Cost = SequentialReadCost × N_pages + RandomReadCost × N_random
CPU Cost = RowProcessCost × N_rows + VectorProcessCost × N_vectors
```

**影响因素：**
- **IO 成本**：读取页数、顺序 vs 随机
- **CPU 成本**：每行处理开销、向量化加速
- **内存成本**：哈希表大小、排序内存
- **网络成本**：分布式查询的数据传输量

**统计信息：**
- 表行数、列基数（Cardinality）
- 直方图（Histogram）：数据分布
- 采样统计：近似但不精确

---

## 5. 数据压缩

### Q11: 列式存储中常用的数据压缩算法有哪些？

**答案：**

1. **字典编码（Dictionary Encoding）**
   - 将重复值映射为整数 ID
   - 适合基数低的列
   ```
   ["北京", "上海", "北京", "深圳"]
   → Dictionary: ["北京", "上海", "深圳"]
   → Encoded: [0, 1, 0, 2]
   ```

2. **位图编码（Bit Map）**
   - 每个唯一值一个位图
   - 适合基数适中的列
   ```
   值 "北京" 的位图: 1010 表示第1、3行是"北京"
   ```

3. **行程编码（Run-Length Encoding, RLE）**
   - 连续相同值存储为 (值, 次数)
   - 适合排序后的列
   ```
   [北京, 北京, 北京, 上海, 上海] → [(北京,3), (上海,2)]
   ```

4. **增量编码（Delta Encoding）**
   - 存储相邻值的差值
   - 适合递增序列（日期、ID）
   ```
   [100, 101, 103, 108] → [100, 1, 2, 5]
   ```

5. **压缩算法**
   - **LZ4**：快速解压，适合热数据
   - **ZSTD**：高压缩比，适合冷数据
   - **Snappy**：平衡速度和压缩率

### Q12: 什么是混合压缩（Hybrid Compression）？

**答案：**

混合压缩根据数据类型选择最佳编码：

```
数据块 Header
├── Encoding Type: 字典 + LZ4
├── Dictionary Size: 1024 entries
└── Compressed Data
    ├── Dictionary: ["北京", "上海", ...]
    └── Encoded Integers: [0,1,0,2,...] → LZ4 压缩
```

**常见组合：**
- 低基数 + 高重复：字典编码 + 位图
- 有序递增：增量编码 + LZ4
- 浮点数：谓词编码 + 差值编码
- 字符串：字典编码 + 前缀压缩

**Parquet 的实现：**
```cpp
// 伪代码
for (column : columns) {
    if (column.isLowCardinality()) {
        encoding = DICTIONARY;
    } else if (column.isSorted()) {
        encoding = DELTA;
    } else {
        encoding = RLE;
    }
    compressed_data = applyEncoding(encoding, column);
}
```

---

## 6. 性能优化

### Q13: 如何优化 OLAP 查询性能？

**答案：**

1. **分区（Partitioning）**
   - 按时间/地区分区
   - 减少扫描数据量
   ```sql
   SELECT * FROM sales WHERE date >= '2024-01-01'
   -- 只扫描 2024 分区
   ```

2. **排序键（Sort Key / Clustering）**
   - 将高频过滤列放在前面
   - 利用范围裁剪
   ```sql
   CREATE TABLE t (a int, b int) ORDER BY (a, b);
   ```

3. **物化视图/预聚合**
   - 预计算高频查询
   - 将复杂聚合转化为简单查询

4. **索引（稀疏索引）**
   - 每个 Row Group 的 Min-Max 统计
   - 跳过不相关数据块

5. **资源隔离**
   - 大查询使用独立资源池
   - 避免影响实时查询

### Q14: 什么是 Late Materialization？

**答案：**

Late Materialization 是一种优化策略：先处理轻量级数据（索引、位置），最后才获取完整列数据。

**示例：查询 `SELECT name FROM users WHERE age > 30`**

```
传统方式：
1. 读取 age 列 → 过滤
2. 读取 name 列 → 过滤后的行

Late Materialization：
1. 读取 age 列 → 过滤 → 得到位置列表 [1, 5, 8, ...]
2. 只从 name 列读取位置列表对应的行
   （利用 SIMD 批量读取）
```

**优势：**
- 减少 IO：只读取必要的列
- 减少内存：中间结果只存储位置
- 适合高选择率查询

---

## 7. 相关代码位置

| 功能 | 代码路径 |
|------|---------|
| 列式存储实现 | `src/algo/quantization/` |
| SIMD 距离计算 | `src/algo/distance/` |
| K-Means 聚类 | `src/algo/Kmeans/` |
| 数据压缩 | `src/algo/quantization/pq.c, sq.c` |
| 向量索引 | `src/db/index/vector_index/` |

---

*文档版本：v1.0*
*最后更新：2024-12-20*
