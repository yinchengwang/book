## ADDED Requirements

### Requirement: 标记删除向量
系统 SHALL 提供 `faiss_ivf_index_remove_ids(index, ids_to_remove, n_remove)` 将指定向量在线性存储和倒排桶中标记为已删除（id = -1 作为墓碑）。
系统 SHALL 同步清除 DirectMap 中对应条目。

#### Scenario: 删除单个向量
- **WHEN** 索引有 100 个向量，调用 `remove_ids(index, [42], 1)`
- **THEN** 返回删除数量 1，向量 42 在 vectors 数组和对应桶中 id 均标记为 -1，DirectMap[42] = -1

#### Scenario: 删除不存在的向量
- **WHEN** 调用 `remove_ids(index, [999], 1)` 但 id=999 不在索引中或已是墓碑
- **THEN** 返回 0（实际删除数），无错误

#### Scenario: 批量删除
- **WHEN** 调用 `remove_ids(index, [10, 20, 30], 3)`
- **THEN** 3 个向量均标记为墓碑，DirectMap 对应条目清除，n_total 减 3

### Requirement: 搜索时跳过墓碑
系统 SHALL 在 IVF 搜索遍历桶内向量时跳过 id 为 -1 的墓碑条目。

#### Scenario: 含墓碑的桶内搜索
- **WHEN** 桶内有 [5, -1, 10, -1, 15] 五个条目，搜索 k=3
- **THEN** 只对 id=5, 10, 15 计算距离，-1 条目自动跳过，结果不包含 -1

### Requirement: 重构向量
系统 SHALL 提供 `faiss_ivf_index_reconstruct(index, id, recons)` 通过 DirectMap 找到向量在桶中的位置，解码残差编码并加上对应二级中心，还原原始向量。

#### Scenario: 重构已编码向量
- **WHEN** 向量 id=5 在 list=2 offset=3 位置，带有 PQ 编码，调用 `reconstruct(index, 5, buf)`
- **THEN** buf 中写入还原后的 float 向量，与原向量误差 < 1e-3

#### Scenario: 重构不存在的向量
- **WHEN** id=999 不在 DirectMap 中时调用 `reconstruct(index, 999, buf)`
- **THEN** 返回错误码 -1

### Requirement: 真空清理墓碑
系统 SHALL 提供 `faiss_ivf_index_compact(index)` 遍历所有桶，将有效元素（id != -1）前移压缩，释放尾部空间，并更新 DirectMap 中的 offset 偏移。

#### Scenario: 压缩含墓碑的桶
- **WHEN** 桶内有 [5, -1, 10, -1, 15]，调用 compact
- **THEN** 桶变为 [5, 10, 15]，size 从 5 缩至 3，DirectMap 中 id=10 的 offset 从 2 更新为 1
