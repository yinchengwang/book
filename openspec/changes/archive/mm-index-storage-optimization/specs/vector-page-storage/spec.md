# 向量分页存储规范

## ADDED Requirements

### Requirement: 向量页池管理

向量存储引擎 SHALL 支持基于固定大小页面的分页管理机制。页面大小可配置为 4KB、8KB 或 16KB，每页存储固定数量的向量数据。

#### Scenario: 创建向量页池
- **WHEN** 调用 `vector_page_pool_create(data_dir, max_pages, page_size, vector_dim)`
- **THEN** 系统 SHALL 创建向量页池并返回句柄
- **AND** 系统 SHALL 根据 page_size 和 vector_dim 计算每页向量容量

#### Scenario: 追加向量到页池
- **WHEN** 调用 `vector_page_append(pool, vector, vector_id)`
- **THEN** 系统 SHALL 找到有空闲空间的页面
- **OR** 系统 SHALL 分配新页面（当 page_count < max_pages 时）
- **AND** 系统 SHALL 将向量写入页面的下一个空闲槽

#### Scenario: 页面置换
- **WHEN** 页池已满且需要分配新页面
- **THEN** 系统 SHALL 使用 Clock-Sweep 算法选择候选页面
- **AND** 系统 SHALL 将脏页刷盘后再驱逐
- **AND** 系统 SHALL 返回被驱逐页面的 ID

#### Scenario: 页面刷盘
- **WHEN** 调用 `vector_page_flush(pool, page_id)` 且页面为脏
- **THEN** 系统 SHALL 将页面数据写入 `vector_pages.dat` 文件
- **AND** 系统 SHALL 清除页面 dirty 标志
- **AND** 系统 SHALL 更新元数据文件

### Requirement: 内存映射支持

向量页池 SHALL 支持 mmap 内存映射模式，减少内存拷贝开销。

#### Scenario: 启用 mmap 模式
- **WHEN** 创建页池时设置 mmap 标志为 true
- **THEN** 系统 SHALL 使用 mmap 将数据文件映射到内存
- **AND** 页面读取 SHALL 直接从映射内存读取，无需 fread

#### Scenario: mmap 页置换
- **WHEN** mmap 模式下需要驱逐页面
- **THEN** 系统 SHALL 仅驱逐修改过的页面
- **AND** 未修改页面 SHALL 保持 mmap 映射状态

### Requirement: 页面统计信息

向量页池 SHALL 提供完整的统计信息用于监控和调优。

#### Scenario: 获取页池统计
- **WHEN** 调用 `vector_page_get_stats(pool, &stats)`
- **THEN** 系统 SHALL 返回 page_count、hits、misses、evictions、flushes
- **AND** 系统 SHALL 计算并返回缓存命中率 hit_rate

### Requirement: 向量页文件格式

向量页文件 SHALL 使用固定格式以支持增量读取和修复。

```
Offset 0: VectorPageHeader (64 bytes)
  - magic: 0x5650 ("VP")
  - version: uint16_t
  - page_id: int32_t
  - vector_count: int32_t
  - capacity: int32_t
  - next_page: int32_t
  - lsn: uint64_t

Offset 64: Vector Data
  - vector_ids: int32_t[capacity]
  - vector_data: float[capacity * dimension]

最后 16 bytes: Page Footer
  - checksum: uint32_t
  - reserved: uint32_t
  - magic_end: 0x5056 ("VP" 反转)
```

#### Scenario: 验证页面完整性
- **WHEN** 加载页面时
- **THEN** 系统 SHALL 验证 magic 字段
- **AND** 系统 SHALL 验证 checksum
- **AND** 如验证失败，系统 SHALL 返回错误
