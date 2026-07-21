/**
 * @file catalog.h
 * @brief Catalog 系统公共接口
 *
 * Catalog 是数据库的系统表集合，存储所有数据库对象的元数据。
 * 参考 PostgreSQL 的 pg_class, pg_attribute, pg_index 等系统表。
 */
#ifndef DB_CATALOG_H
#define DB_CATALOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 对象标识符类型 */
typedef uint32_t Oid;

/** 无效的 OID */
#define InvalidOid ((Oid)0)

/** OID 最大值 */
#define MaxOid UINT32_MAX

/** 名称最大长度（与 PostgreSQL 一致） */
#define NAMEDATALEN 64

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct table_info_s table_info_t;
typedef struct column_info_s column_info_t;
typedef struct index_info_s index_info_t;
typedef struct column_def_s column_def_t;

/* ============================================================
 * 表信息结构 (pg_class)
 * ============================================================ */

/**
 * 分区类型
 */
typedef enum PartitionStrategy {
    PARTITION_STRATEGY_NONE = 0,   /**< 非分区表 */
    PARTITION_STRATEGY_RANGE,      /**< 范围分区 */
    PARTITION_STRATEGY_LIST,       /**< 列表分区 */
    PARTITION_STRATEGY_HASH,       /**< 哈希分区 */
} PartitionStrategy;

/**
 * @brief 表信息结构
 *
 * 对应 pg_class 系统表
 */
struct table_info_s {
    Oid         oid;             /**< 表 OID */
    char        name[NAMEDATALEN]; /**< 表名 */
    Oid         namespace_oid;   /**< 命名空间 OID */
    Oid         type_oid;        /**< 行类型 OID */
    char        relkind;         /**< 种类：r=表, i=索引, v=视图, p=分区表 */
    int16_t       natts;           /**< 用户列数 */
    Oid         filenode;        /**< 物理文件节点 */
    Oid         tablespace;      /**< 表空间 OID */
    int32_t       npages;          /**< 页面数估计 */
    float       ntupes;          /**< 元组数估计 */
    Oid         owner;           /**< 所有者 OID */
    char        persistence;     /**< 持久性：p=永久, t=临时 */
    int16_t       nchecks;         /**< CHECK 约束数 */
    bool        has_index;       /**< 是否有索引 */
    bool        has_pkey;        /**< 是否有主键 */

    /* 分区表相关 */
    PartitionStrategy part_strategy; /**< 分区策略 */
    int16_t           partnatts;     /**< 分区键列数 */
    int16_t           partattrs[4];  /**< 分区键列号（最多 4 列） */
    Oid               parent_oid;    /**< 父表 OID（分区表为父表，分区为子表） */
    Oid               *part_oids;    /**< 分区子表 OID 数组 */
    int               nparts;        /**< 分区子表数量 */
};

/* ============================================================
 * 列信息结构 (pg_attribute)
 * ============================================================ */

/**
 * @brief 列信息结构
 *
 * 对应 pg_attribute 系统表
 */
struct column_info_s {
    Oid         table_oid;       /**< 所属表 OID */
    char        name[NAMEDATALEN]; /**< 列名 */
    Oid         type_oid;        /**< 数据类型 OID */
    int16_t       attnum;          /**< 列序号 (从 1 开始) */
    int16_t       len;             /**< 类型长度 */
    int32_t       typmod;          /**< 类型修饰符 */
    bool        attnotnull;      /**< NOT NULL 约束 */
    bool        has_default;     /**< 是否有默认值 */
    bool        is_dropped;      /**< 是否已删除 */
    char        align;           /**< 对齐要求 */
    char        storage;         /**< 存储策略 */
};

/* ============================================================
 * 索引信息结构 (pg_index)
 * ============================================================ */

/**
 * @brief 索引信息结构
 *
 * 对应 pg_index 系统表
 */
struct index_info_s {
    Oid         oid;             /**< 索引 OID */
    Oid         table_oid;       /**< 被索引表 OID */
    char        name[NAMEDATALEN]; /**< 索引名 */
    int16_t       nkeys;           /**< 索引键数 */
    int16_t       *key_nums;       /**< 键列位置数组 */
    bool        is_unique;       /**< 是否唯一索引 */
    bool        is_primary;      /**< 是否主键索引 */
    bool        is_valid;        /**< 索引是否有效 */
};

/* ============================================================
 * 列定义（用于创建表/索引）
 * ============================================================ */

/**
 * @brief 列定义
 *
 * 用于 CREATE TABLE 和 ALTER TABLE ADD COLUMN
 */
struct column_def_s {
    char        name[NAMEDATALEN]; /**< 列名 */
    Oid         type_oid;        /**< 数据类型 OID */
    int32_t       typmod;          /**< 类型修饰符 */
    bool        not_null;        /**< NOT NULL */
    bool        has_default;     /**< 有默认值 */
    void        *default_expr;   /**< 默认值表达式 */
};

/* ============================================================
 * Catalog 结果码
 * ============================================================ */

typedef enum catalog_result_e {
    CATALOG_SUCCESS = 0,        /**< 成功 */
    CATALOG_NOT_FOUND = 1,      /**< 对象不存在 */
    CATALOG_DUPLICATE = 2,      /**< 对象已存在 */
    CATALOG_IN_USE = 3,         /**< 对象被使用中 */
    CATALOG_ERROR = -1          /**< 一般错误 */
} catalog_result_t;

/* ============================================================
 * Catalog 初始化
 * ============================================================ */

/**
 * @brief 初始化 Catalog 系统
 * @return 0 成功，-1 失败
 */
int catalog_init(void);

/**
 * @brief 关闭 Catalog 系统
 */
void catalog_shutdown(void);

/* ============================================================
 * 表操作
 * ============================================================ */

/**
 * @brief 创建新表
 * @param name 表名
 * @param columns 列定义数组
 * @param ncolumns 列数
 * @return 表 OID，失败返回 InvalidOid
 */
Oid catalog_create_table(const char *name, column_def_t *columns, int ncolumns);

/**
 * @brief 删除表
 * @param table_oid 表 OID
 * @return 结果码
 */
catalog_result_t catalog_drop_table(Oid table_oid);

/**
 * @brief 获取表信息
 * @param table_oid 表 OID
 * @return 表信息，失败返回 NULL
 */
table_info_t *catalog_get_table(Oid table_oid);

/**
 * @brief 按名称查找表
 * @param name 表名
 * @return 表 OID，未找到返回 InvalidOid
 */
Oid catalog_lookup_table(const char *name);

/**
 * @brief 获取所有表
 * @param ntables 输出表数量
 * @return 表信息数组，调用者负责释放
 */
table_info_t *catalog_get_all_tables(int *ntables);

/**
 * @brief 释放表信息
 * @param info 表信息
 */
void catalog_free_table(table_info_t *info);

/* ============================================================
 * 列操作
 * ============================================================ */

/**
 * @brief 获取表的列信息
 * @param table_oid 表 OID
 * @param ncolumns 输出列数
 * @return 列信息数组，调用者负责释放
 */
column_info_t *catalog_get_columns(Oid table_oid, int *ncolumns);

/**
 * @brief 添加列
 * @param table_oid 表 OID
 * @param column 列定义
 * @return 结果码
 */
catalog_result_t catalog_add_column(Oid table_oid, column_def_t *column);

/**
 * @brief 删除列
 * @param table_oid 表 OID
 * @param column_name 列名
 * @return 结果码
 */
catalog_result_t catalog_drop_column(Oid table_oid, const char *column_name);

/**
 * @brief 释放列信息数组
 * @param columns 列信息数组
 */
void catalog_free_columns(column_info_t *columns);

/* ============================================================
 * 索引操作
 * ============================================================ */

/**
 * @brief 创建索引
 * @param index_name 索引名
 * @param table_oid 表 OID
 * @param columns 列名数组
 * @param ncolumns 列数
 * @param is_unique 是否唯一
 * @return 索引 OID，失败返回 InvalidOid
 */
Oid catalog_create_index(const char *index_name, Oid table_oid,
                         const char **columns, int ncolumns, bool is_unique);

/**
 * @brief 删除索引
 * @param index_oid 索引 OID
 * @return 结果码
 */
catalog_result_t catalog_drop_index(Oid index_oid);

/**
 * @brief 获取表的索引列表
 * @param table_oid 表 OID
 * @param nindexes 输出索引数
 * @return 索引信息数组，调用者负责释放
 */
index_info_t *catalog_get_indexes(Oid table_oid, int *nindexes);

/**
 * @brief 获取索引信息
 * @param index_oid 索引 OID
 * @return 索引信息，失败返回 NULL
 */
index_info_t *catalog_get_index(Oid index_oid);

/**
 * @brief 释放索引信息
 * @param info 索引信息
 */
void catalog_free_index(index_info_t *info);

/* ============================================================
 * 缓存管理
 * ============================================================ */

/**
 * @brief 使表缓存失效
 * @param table_oid 表 OID
 */
void catalog_invalidate_table(Oid table_oid);

/**
 * @brief 使所有缓存失效
 */
void catalog_invalidate_all(void);

/**
 * @brief 获取缓存统计信息
 * @param hits 输出命中次数
 * @param misses 输出未命中次数
 */
void catalog_get_cache_stats(uint64_t *hits, uint64_t *misses);

/* ============================================================
 * 分区表操作
 * ============================================================ */

/**
 * @brief 创建分区表
 *
 * @param name       分区表名
 * @param columns    列定义数组
 * @param ncolumns   列数
 * @param strategy   分区策略
 * @param part_attrs 分区键列号数组（1-based）
 * @param npart_attrs 分区键列数
 * @return 表 OID，失败返回 InvalidOid
 */
Oid catalog_create_partitioned_table(const char *name,
                                     column_def_t *columns, int ncolumns,
                                     PartitionStrategy strategy,
                                     int16_t *part_attrs, int npart_attrs);

/**
 * @brief 创建分区（分区子表）
 *
 * @param parent_oid  父分区表 OID
 * @param part_name   分区名
 * @param bound_val   分区边界值
 * @return 分区 OID，失败返回 InvalidOid
 */
Oid catalog_create_partition(Oid parent_oid, const char *part_name,
                             int64_t bound_val);

/**
 * @brief 获取分区表的子分区列表
 *
 * @param table_oid  分区表 OID
 * @param nparts     输出分区数量
 * @return 分区 OID 数组，调用者负责释放；失败返回 NULL
 */
Oid *catalog_get_partitions(Oid table_oid, int *nparts);

/**
 * @brief 获取分区所属的父表 OID
 *
 * @param part_oid 分区 OID
 * @return 父表 OID，非分区表返回 InvalidOid
 */
Oid catalog_get_partition_parent(Oid part_oid);

/* ============================================================
 * 视图管理
 * ============================================================ */

/**
 * @brief 创建视图
 * @param view_name 视图名
 * @param query_def 查询定义（SQL 字符串）
 * @return 视图 OID，失败返回 InvalidOid
 */
Oid catalog_create_view(const char *view_name, const char *query_def);

/**
 * @brief 删除视图
 * @param view_oid 视图 OID
 * @return 0 成功，-1 失败
 */
int catalog_drop_view(Oid view_oid);

/**
 * @brief 获取视图定义
 * @param view_oid 视图 OID
 * @return 查询定义字符串（内部存储，不需要释放）
 */
const char *catalog_get_view_definition(Oid view_oid);

/**
 * @brief 检查是否为视图
 * @param oid 关系 OID
 * @return true 是视图，false 不是
 */
bool catalog_is_view(Oid oid);

/* ============================================================
 * 物化视图管理
 * ============================================================ */

/**
 * @brief 创建物化视图
 * @param mv_name 物化视图名
 * @param query_def 查询定义
 * @return 物化视图 OID，失败返回 InvalidOid
 */
Oid catalog_create_matview(const char *mv_name, const char *query_def);

/**
 * @brief 删除物化视图
 * @param mv_oid 物化视图 OID
 * @return 0 成功，-1 失败
 */
int catalog_drop_matview(Oid mv_oid);

/**
 * @brief 刷新物化视图
 * @param mv_oid 物化视图 OID
 * @param wait 是否等待完成
 * @return 0 成功，-1 失败
 */
int catalog_refresh_matview(Oid mv_oid, bool wait);

/**
 * @brief 获取物化视图刷新时间
 * @param mv_oid 物化视图 OID
 * @return 最后刷新时间戳（Unix 时间），从未刷新返回 0
 */
uint64_t catalog_get_matview_refresh_time(Oid mv_oid);

/**
 * @brief 检查是否为物化视图
 * @param oid 关系 OID
 * @return true 是物化视图，false 不是
 */
bool catalog_is_matview(Oid oid);

/* ============================================================
 * 继承表管理
 * ============================================================ */

/**
 * @brief 创建继承表
 * @param child_name 子表名
 * @param parent_oid 父表 OID
 * @return 子表 OID，失败返回 InvalidOid
 */
Oid catalog_create_inherited_table(const char *child_name, Oid parent_oid);

/**
 * @brief 获取表的父表
 * @param child_oid 子表 OID
 * @return 父表 OID，非继承表返回 InvalidOid
 */
Oid catalog_get_parent_table(Oid child_oid);

/**
 * @brief 获取子表列表
 * @param parent_oid 父表 OID
 * @param nchildren 输出子表数量
 * @return 子表 OID 数组，调用者负责释放
 */
Oid *catalog_get_child_tables(Oid parent_oid, int *nchildren);

/* ============================================================
 * 规则系统
 * ============================================================ */

/** 规则事件类型 */
typedef enum RuleEvent_e {
    RULE_EVENT_INSERT = 1,   /**< INSERT 事件 */
    RULE_EVENT_UPDATE = 2,   /**< UPDATE 事件 */
    RULE_EVENT_DELETE = 3,   /**< DELETE 事件 */
    RULE_EVENT_SELECT = 4    /**< SELECT 事件（INSTEAD OF） */
} RuleEvent;

/**
 * @brief 规则信息
 */
typedef struct RuleInfo_s {
    Oid         relation_oid;     /**< 规则所属关系 */
    const char *rule_name;        /**< 规则名 */
    RuleEvent   event;            /**< 事件类型 */
    const char *action;           /**< 规则动作（SQL 语句） */
    bool        is_instead;       /**< 是否为 INSTEAD 规则 */
    bool        is_enabled;       /**< 是否启用 */
} RuleInfo;

/**
 * @brief 创建规则
 * @param rel_oid 关系 OID
 * @param rule_name 规则名
 * @param event 事件类型
 * @param action 规则动作
 * @param is_instead 是否为 INSTEAD 规则
 * @return 规则 ID，失败返回 -1
 */
int catalog_create_rule(Oid rel_oid, const char *rule_name,
                        RuleEvent event, const char *action, bool is_instead);

/**
 * @brief 删除规则
 * @param rule_id 规则 ID
 * @return 0 成功，-1 失败
 */
int catalog_drop_rule(int rule_id);

/**
 * @brief 获取关系的规则列表
 * @param rel_oid 关系 OID
 * @param event 事件类型（过滤，可为 -1 表示所有）
 * @param nrules 输出规则数量
 * @return 规则信息数组，调用者负责释放
 */
RuleInfo *catalog_get_rules(Oid rel_oid, RuleEvent event, int *nrules);

#ifdef __cplusplus
}
#endif

#endif /* DB_CATALOG_H */
