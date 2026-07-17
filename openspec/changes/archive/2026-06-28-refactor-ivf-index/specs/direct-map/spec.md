## ADDED Requirements

### Requirement: 创建和初始化 DirectMap
系统 SHALL 提供 `faiss_direct_map_init(dm, type, capacity)` 初始化指定类型和大小的 DirectMap。
type=1 时分配 capacity 大小的 int64_t 数组，所有槽初始值为 -1（表示不存在）。
type=0 时不做任何分配。

#### Scenario: 初始化数组型 DirectMap
- **WHEN** 调用 `faiss_direct_map_init(&dm, 1, 10000)`
- **THEN** dm.type=1，dm.array 长度为 10000，所有值为 -1，dm.size=10000

#### Scenario: 初始化空 DirectMap
- **WHEN** 调用 `faiss_direct_map_init(&dm, 0, 0)`
- **THEN** dm.type=0，dm.array 为 NULL

### Requirement: 添加映射
系统 SHALL 提供 `faiss_direct_map_add(dm, id, list_no, offset)` 将向量 ID 映射到 (list_no << 32 | offset)。
若 id 超出数组范围，数组 SHALL 自动扩容至 id × 2。

#### Scenario: 正常添加映射
- **WHEN** 调用 `direct_map_add(&dm, 5, 2, 3)`
- **THEN** dm.array[5] = 0x0000000200000003，低 32 位=3（offset），高 32 位=2（list_no）

#### Scenario: 覆盖已有映射
- **WHEN** 对已存在映射的 id=5 调用 `direct_map_add(&dm, 5, 7, 1)`
- **THEN** dm.array[5] 更新为 0x0000000700000001

### Requirement: 查询映射
系统 SHALL 提供 `faiss_direct_map_get(dm, id)` 返回对应向量 ID 的编码位置。
若 id 不存在（值为 -1 或超范围），返回 -1。

#### Scenario: 查询存在映射的向量
- **WHEN** 已添加 (5 → list=2, offset=3) 后调用 `direct_map_get(&dm, 5)`
- **THEN** 返回 0x0000000200000003

#### Scenario: 查询不存在映射的向量
- **WHEN** id=99 从未添加时调用 `direct_map_get(&dm, 99)`
- **THEN** 返回 -1

### Requirement: 删除映射
系统 SHALL 提供 `faiss_direct_map_remove(dm, id)` 将指定 id 的映射值重置为 -1。

#### Scenario: 删除后查询返回 -1
- **WHEN** 先添加 (10 → list=1, offset=5)，再调用 `direct_map_remove(&dm, 10)`，再查询 id=10
- **THEN** `direct_map_get` 返回 -1

### Requirement: 清空 DirectMap
系统 SHALL 提供 `faiss_direct_map_clear(dm)` 将所有数组槽位重置为 -1。

### Requirement: 释放 DirectMap
系统 SHALL 提供 `faiss_direct_map_drop(dm)` 释放 array 并重置所有字段为 0/NULL。

#### Scenario: 销毁后再次使用
- **WHEN** 调用 `direct_map_drop(&dm)` 后再调用 `direct_map_init(&dm, 1, 500)`
- **THEN** 正常分配，无内存泄漏
