# Catalog 系统规格

## 1. 概述

Catalog 是数据库的系统表集合，存储所有数据库对象的元数据（表、列、索引、函数、类型等）。

## 2. 核心系统表

### 2.1 pg_class

存储所有 relations（表、索引、视图、物化视图、序列）的信息。

| 列名 | 类型 | 说明 |
|------|------|------|
| oid | oid | 行标识符 |
| relname | name | 关系名称 |
| relnamespace | oid | 命名空间 OID |
| reltype | oid | 行类型 OID |
| relkind | char | 关系种类：r=表, i=索引, v=视图, S=序列 |
| relnatts | int16 | 用户列数 |
| relfilenode | oid | 物理存储文件节点 |
| reltablespace | oid | 表空间 OID |
| relpages | int4 | 页面数量估计 |
| reltuples | float4 | 元组数量估计 |
| relowner | oid | 所有者 OID |
| relpersistence | char | 持久性：p=永久, t=临时, u=非日志 |
| relchecks | int16 | CHECK 约束数量 |
| relhasindex | bool | 是否有索引 |
| relisshared | bool | 是否共享表 |
| relhaspkey | bool | 是否有主键 |
| relhasrules | bool | 是否有规则 |
| relhastriggers | bool | 是否有触发器 |

### 2.2 pg_attribute

存储所有表的列定义。

| 列名 | 类型 | 说明 |
|------|------|------|
| attrelid | oid | 所属表 OID |
| attname | name | 列名 |
| atttypid | oid | 数据类型 OID |
| attstattarget | int4 | 统计目标 |
| attlen | int2 | 类型长度 |
| attnum | int2 | 列序号 (从 1 开始) |
| attndims | int4 | 数组维度 |
| attcacheoff | int4 | 缓存偏移量 |
| atttypmod | int4 | 类型修饰符 |
| attbyval | bool | 值是否按值传递 |
| attstorage | char | 存储策略 |
| attalign | char | 对齐要求 |
| attnotnull | bool | 是否 NOT NULL |
| atthasdef | bool | 是否有默认值 |
| attisdropped | bool | 是否已删除 |
| attislocal | bool | 是否本地定义 |
| attinhcount | int4 | 继承数量 |

### 2.3 pg_index

存储索引信息。

| 列名 | 类型 | 说明 |
|------|------|------|
| indexrelid | oid | 索引 OID |
| indrelid | oid | 被索引表 OID |
| indnatts | int16 | 索引列数 |
| indnkeyatts | int16 | 索引键数 |
| indisunique | bool | 是否唯一 |
| indisprimary | bool | 是否主键 |
| indisexclusion | bool | 是否排他约束 |
| indimmediate | bool | 是否立即检查 |
| indisclustered | bool | 是否按此索引聚簇 |
| indisvalid | bool | 是否有效 |
| indcheckxmin | bool | 是否检查 xmin |
| indisready | bool | 是否就绪 |
| indislive | bool | 是否存活 |
| indkey | int2vector | 索引列位置 |
| indcollation | oidvector | 索引列排序规则 |
| indclass | oidvector | 索引列操作符类 |
| indoption | int2vector | 索引列选项 |
| indexprs | pg_node_tree | 索引表达式 |
| indpred | pg_node_tree | 索引谓词 |

### 2.4 pg_database

存储数据库信息。

| 列名 | 类型 | 说明 |
|------|------|------|
| oid | oid | 行标识符 |
| datname | name | 数据库名称 |
| datdba | oid | 所有者 |
| encoding | int4 | 字符编码 |
| datcollate | name | 排序规则 |
| datctype | name | 字符分类 |
| datistemplate | bool | 是否模板 |
| datallowconn | bool | 是否允许连接 |
| datconnlimit | int4 | 连接数限制 |
| datfrozenxid | xid | 冻结事务 ID |
| datminmxid | xid | 最小多事务 ID |
| dattablespace | oid | 默认表空间 |

## 3. API 规格

### 3.1 初始化

```c
/**
 * @brief 初始化 Catalog 系统
 * @return 0 成功，-1 失败
 */
int catalog_init(void);

/**
 * @brief 关闭 Catalog 系统
 */
void catalog_shutdown(void);
```

### 3.2 表操作

```c
/**
 * @brief 创建新表
 * @param name 表名
 * @param columns 列定义数组
 * @param ncolumns 列数
 * @return 表 OID，失败返回 0
 */
uint32_t catalog_create_table(const char *name, column_def_t *columns, int ncolumns);

/**
 * @brief 删除表
 * @param table_oid 表 OID
 * @return 0 成功，-1 失败
 */
int catalog_drop_table(uint32_t table_oid);

/**
 * @brief 获取表信息
 * @param table_oid 表 OID
 * @return 表信息结构，失败返回 NULL
 */
table_info_t *catalog_get_table(uint32_t table_oid);

/**
 * @brief 按名称查找表
 * @param name 表名
 * @return 表 OID，未找到返回 0
 */
uint32_t catalog_lookup_table(const char *name);
```

### 3.3 列操作

```c
/**
 * @brief 获取表的列信息
 * @param table_oid 表 OID
 * @param ncolumns 输出列数
 * @return 列信息数组，调用者负责释放
 */
column_info_t *catalog_get_columns(uint32_t table_oid, int *ncolumns);

/**
 * @brief 添加列
 * @param table_oid 表 OID
 * @param column 列定义
 * @return 0 成功，-1 失败
 */
int catalog_add_column(uint32_t table_oid, column_def_t *column);

/**
 * @brief 删除列
 * @param table_oid 表 OID
 * @param column_name 列名
 * @return 0 成功，-1 失败
 */
int catalog_drop_column(uint32_t table_oid, const char *column_name);
```

### 3.4 索引操作

```c
/**
 * @brief 创建索引
 * @param index_name 索引名
 * @param table_oid 表 OID
 * @param columns 列名数组
 * @param ncolumns 列数
 * @param unique 是否唯一
 * @return 索引 OID，失败返回 0
 */
uint32_t catalog_create_index(const char *index_name, uint32_t table_oid,
                              const char **columns, int ncolumns, bool unique);

/**
 * @brief 删除索引
 * @param index_oid 索引 OID
 * @return 0 成功，-1 失败
 */
int catalog_drop_index(uint32_t index_oid);

/**
 * @brief 获取表的索引列表
 * @param table_oid 表 OID
 * @param nindexes 输出索引数
 * @return 索引信息数组
 */
index_info_t *catalog_get_indexes(uint32_t table_oid, int *nindexes);
```

## 4. 数据结构

### 4.1 表信息

```c
typedef struct table_info_s {
    uint32_t oid;                  // 表 OID
    char name[NAMEDATALEN];        // 表名
    uint32_t namespace_oid;        // 命名空间
    char kind;                     // 种类
    int nattrs;                    // 列数
    uint32_t filenode;             // 文件节点
    uint32_t owner;                // 所有者
    // ... 其他字段
} table_info_t;
```

### 4.2 列信息

```c
typedef struct column_info_s {
    uint32_t table_oid;            // 所属表 OID
    char name[NAMEDATALEN];        // 列名
    uint32_t type_oid;             // 数据类型 OID
    int16 attnum;                  // 列序号
    int16 len;                     // 类型长度
    int attnotnull;                // NOT NULL
    bool has_default;              // 有默认值
    // ... 其他字段
} column_info_t;
```

### 4.3 索引信息

```c
typedef struct index_info_s {
    uint32_t oid;                  // 索引 OID
    uint32_t table_oid;            // 所属表 OID
    char name[NAMEDATALEN];        // 索引名
    int ncolumns;                  // 列数
    int *column_nums;              // 列序号数组
    bool is_unique;                // 是否唯一
    bool is_primary;               // 是否主键
    // ... 其他字段
} index_info_t;
```

## 5. 内部实现

### 5.1 Catalog Cache

```
┌─────────────────────────────────────────┐
│           Catalog Cache                 │
├─────────────┬─────────────┬─────────────┤
│ pg_class    │ pg_attribute│  pg_index   │
│   Cache     │   Cache     │   Cache     │
├─────────────┴─────────────┴─────────────┤
│           OidCache Hash Table           │
│  (OID → 内存对象 映射)                   │
└─────────────────────────────────────────┘
```

### 5.2 缓存失效

当系统表发生变更时，需要使对应缓存失效：
- INSERT → 添加缓存条目
- UPDATE → 更新/失效缓存条目
- DELETE → 删除缓存条目

## 6. 限制

- 不支持命名空间（统一使用 public 命名空间）
- 不支持表空间（统一使用默认表空间）
- 不支持继承（未来扩展）
- 不支持分区（未来扩展）
