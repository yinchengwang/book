## ADDED Requirements

### Requirement: 索引类型枚举

SDK SHALL 提供 `IndexType` 枚举类，允许用户在选择索引算法时使用类型安全的常量。

#### Scenario: IndexType 常量
- **WHEN** 用户导入 `from minivecdb import IndexType`
- **THEN** `IndexType` SHALL 包含以下属性：
  - `IndexType.HNSW = 0`
  - `IndexType.DISKANN = 1`
  - `IndexType.IVF = 2`
  - `IndexType.BRUTE_FORCE = 3`
  - `IndexType.AUTO = 99`

#### Scenario: create_collection 传 index_type
- **WHEN** 调用 `client.create_collection(name, dimension, index_type=IndexType.HNSW)`
- **THEN** POST /collections 请求体 SHALL 包含 `"index_type": 0`

### Requirement: 集合信息返回 index_type

SDK SHALL 在集合信息中暴露 `index_type` 字段。

#### Scenario: collection.info
- **WHEN** 访问 `collection.info`
- **THEN** 返回的 dict SHALL 包含 `"index_type"` 字段
- **THEN** `"index_type"` SHALL 为整数值，与 `IndexType` 常量对应

## MODIFIED Requirements

### Requirement: 向量操作

系统 SHALL 提供向量操作接口。

#### Scenario: 创建集合
- **WHEN** 调用 `client.create_collection(name, dimension, metric_type, index_type)`
- **THEN** HTTP 请求 SHALL 包含 `index_type` 参数
- **THEN** `index_type` 默认为 `IndexType.AUTO`
- **THEN** 返回集合信息 SHALL 包含 `index_type`

#### Scenario: 插入向量
- **WHEN** 调用 `collection.insert(vectors, ids, metadata)`
- **THEN** 浮点数组 SHALL 被发送到服务器
- **THEN** 行为与改前一致

#### Scenario: 向量搜索
- **WHEN** 调用 `collection.search(query, top_k, filter)`
- **THEN** 搜索结果 SHALL 包含 ID 和距离
- **THEN** 行为与改前一致
