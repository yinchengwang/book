## ADDED Requirements

### Requirement: 创建和销毁倒排表
系统 SHALL 提供 `faiss_inverted_lists_create(nlist, code_size)` 分配 nlist 个空桶，每个桶的 ids/codes 数组初始为空。
系统 SHALL 提供 `faiss_inverted_lists_drop(il)` 释放所有桶及其内部数组所占用的内存。

#### Scenario: 创建无编码倒排表
- **WHEN** 调用 `faiss_inverted_lists_create(8, 0)`
- **THEN** 返回的倒排表有 8 个桶，每个桶的 ids 容量为 0，sizes 和 caps 全为 0，codes 指针全为 NULL

#### Scenario: 创建含编码倒排表
- **WHEN** 调用 `faiss_inverted_lists_create(4, 32)`
- **THEN** code_size 设为 32，每个桶的 codes 数组初始为 NULL

#### Scenario: 释放已创建的倒排表
- **WHEN** 调用 `faiss_inverted_lists_drop(il)` 传入已添加元素的倒排表
- **THEN** 所有桶的 ids/codes 数组和元数据数组均被释放，il 指针置为 NULL

### Requirement: 查询桶信息
系统 SHALL 提供内联函数 `faiss_inverted_lists_list_size(il, list_no)` 返回指定桶的当前元素数。
系统 SHALL 提供内联函数 `faiss_inverted_lists_get_ids(il, list_no)` 返回指定桶的 ID 数组只读指针。
系统 SHALL 提供内联函数 `faiss_inverted_lists_get_codes(il, list_no)` 返回指定桶的编码数组只读指针，若 code_size 为 0 则返回 NULL。

#### Scenario: 查询有数据的桶
- **WHEN** 桶 0 已添加 5 个元素后调用 `list_size(il, 0)`
- **THEN** 返回 5

#### Scenario: 查询空桶
- **WHEN** 桶 3 未添加任何元素时调用 `list_size(il, 3)`
- **THEN** 返回 0

### Requirement: 向桶中添加元素
系统 SHALL 提供 `faiss_inverted_lists_add_entry(il, list_no, id, code)` 向指定桶追加一个元素。
若桶当前容量不足，系统 SHALL 自动扩容（2× 扩容，初始容量 32）。

#### Scenario: 首次添加触发容量分配
- **WHEN** 向空桶 list_no=0 调用 `add_entry(il, 0, 42, NULL)`
- **THEN** 自动分配容量 32，ids[0] = 42，size[0] = 1

#### Scenario: 容量不足时自动扩容
- **WHEN** 桶已满（size == cap = 32）时调用 `add_entry`
- **THEN** 容量扩至 64，新元素正确追加，原有数据不变

#### Scenario: 含编码的添加
- **WHEN** code_size=16 时调用 `add_entry(il, 0, 100, code_data)`
- **THEN** ids[0]=100，codes[0..15] 与 code_data 一致

### Requirement: 手动扩容
系统 SHALL 提供 `faiss_inverted_lists_resize_list(il, list_no, required_size)` 确保桶容量至少为 required_size。
若 needed 值不合理（过大），返回 -1。

#### Scenario: 扩容到指定大小
- **WHEN** 桶容量 32，调用 `resize_list(il, 0, 100)`
- **THEN** 容量 ≥ 100，原有数据保留

#### Scenario: 不需要扩容
- **WHEN** 桶容量 64，调用 `resize_list(il, 0, 30)`
- **THEN** 容量保持 64，数据不变

### Requirement: 清空倒排表
系统 SHALL 提供 `faiss_inverted_lists_reset(il)` 将所有桶的 size 置为 0，但保留已分配容量和 code_size 不变。

#### Scenario: 重置后重新使用
- **WHEN** 对已添加元素的倒排表调用 reset
- **THEN** 所有桶 size=0，容量和 codes 数组保留，可继续添加
