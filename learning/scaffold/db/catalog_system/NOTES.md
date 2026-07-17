# Catalog 系统笔记

## 系统表结构

### pg_class（表信息）
对应 `table_info_t` 结构，存储所有数据库对象（表、索引、视图）的元数据：
- **oid**: 对象的唯一标识符
- **name**: 对象名称
- **namespace_oid**: 所属命名空间（默认为 public=1）
- **relkind**: 对象种类（r=表，i=索引，v=视图）
- **natts**: 用户列数量
- **filenode**: 物理存储文件节点
- **has_index / has_pkey**: 索引和主键标志

### pg_attribute（列信息）
对应 `column_info_t` 结构，存储每个表的列定义：
- **table_oid**: 所属表 OID
- **name**: 列名
- **type_oid**: 数据类型 OID
- **attnum**: 列序号（从 1 开始）
- **attnotnull / has_default**: 约束标志

### pg_index（索引信息）
对应 `index_info_t` 结构，存储索引定义：
- **oid**: 索引 OID
- **table_oid**: 被索引表 OID
- **nkeys**: 索引键数量
- **key_nums**: 键列序号数组
- **is_unique / is_primary**: 唯一性和主键标志

## OID 分配策略

参考 `engineering/src/db/storage/catalog/catalog.c`：

1. **起始值**: next_oid 从 1 开始分配，0 表示 InvalidOid
2. **原子递增**: 每次创建新对象时调用 next_oid++，保证全局唯一
3. **哈希缓存**: 使用 OID % CATALOG_HASH_SIZE 作为桶索引
4. **类型派生**: 表的 type_oid = table_oid + 10000，形成类型与表的一对一映射

## 与参考实现的对比

学习版（scaffold）实现：
- 简化了缓存结构，使用数组替代链表
- 移除了缓存命中率统计
- 列定义直接使用 column_def_t 而非分离的结构

参考实现（engineering）：
- 完整的链表哈希表缓存
- 支持缓存失效（invalidate_table / invalidate_all）
- 支持缓存统计（hits/misses）
- 完整的 catalog_result_t 错误码
