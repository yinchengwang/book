/**
 * @file btreeam.h
 * @brief BTree Access Method 公共接口
 *
 * BTree AM 负责索引的存储和访问。
 * 参考 PostgreSQL 的 nbtree 实现。
 */
#ifndef DB_BTREEAM_H
#define DB_BTREEAM_H

#include "db/rel.h"
#include "db/buf.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** BTree 页面大小（与堆页面相同） */
#define BTREE_PAGE_SIZE 8192

/** 内部节点最小条目数 */
#define BTREE_MIN_ITEMS 4

/** 页面头部大小 */
#define BTREE_PAGE_HEADER_SIZE 20

/** 索引元组大小 */
#define BTREE_ITUPLE_SIZE(index) (sizeof(BTreekirtData) + (index)->nkeys * sizeof(uint32_t))

/* ============================================================
 * 页面类型
 * ============================================================ */

/** BTree 页面类型 */
typedef enum BTPageType_e {
    BTP_LEAF = 0x0001,    /**< 叶子页面 */
    BTP_ROOT = 0x0002,    /**< 根页面 */
    BTP_INTERNAL = 0x0004,/**< 内部页面 */
    BTP_META = 0x0008,    /**< 元数据页面 */
    BTP_DELETED = 0x0010  /**< 已删除页面 */
} BTPageType;

/** BTree 页面标志 */
#define BTP_LEAF_FLAG        0x0001
#define BTP_ROOT_FLAG        0x0002
#define BTP_INTERNAL_FLAG    0x0004
#define BTP_META_FLAG        0x0008
#define BTP_DELETED_FLAG     0x0010
#define BTP_HALF_DEAD        0x0020  /**< 半删除状态 */

/* ============================================================
 * 页面结构
 * ============================================================ */

/**
 * @brief BTree 页面头部
 */
typedef struct BTPageHeaderData {
    uint16_t    btpo_flags;       /**< 页面标志 */
    uint16_t    btpo_level;       /**< 树层级（0=叶子） */
    uint32_t    btpo_prev;        /**< 上一页面 */
    uint32_t    btpo_next;        /**< 下一页面 */
    uint32_t    btpo_xact;        /**< 事务 ID */
    uint16_t    btpo_offset;      /**< 空闲空间起始偏移 */
    uint16_t    btpo_count;       /**< 页面中的条目数 */
} BTPageHeaderData;

/** 页面头部指针 */
typedef BTPageHeaderData *BTPageHeader;

/**
 * @brief BTree 键数据（索引元组）
 */
typedef struct BTreekirtData {
    Oid         heap_node;        /**< 堆页面节点 */
    uint32_t    block_number;     /**< 块号 */
    uint16_t    offset;           /**< 在页面中的偏移 */
    uint16_t    flags;            /**< 标志 */
    /* 键值数据紧随其后 */
} BTreekirtData;

typedef BTreekirtData *BTreekirt;

/**
 * @brief BTree 内部条目（指向子页面）
 */
typedef struct BTInternalTupleData {
    uint32_t    block_number;     /**< 子页面块号 */
    uint16_t    offnum;           /**< 元组偏移 */
    uint8_t     downlink_offset;  /**< 下链偏移 */
    uint8_t     flags;            /**< 标志 */
} BTInternalTupleData;

typedef BTInternalTupleData *BTInternalTuple;

/* ============================================================
 * BTree 操作
 * ============================================================ */

/**
 * @brief 初始化 BTree AM
 * @return 0 成功
 */
int btreeam_init(void);

/**
 * @brief 关闭 BTree AM
 */
void btreeam_shutdown(void);

/**
 * @brief 创建 BTree 索引
 * @param rel 索引 Relation
 * @return 0 成功，-1 失败
 */
int btcreate(Relation rel);

/**
 * @brief 销毁 BTree 索引
 * @param rel 索引 Relation
 * @return 0 成功，-1 失败
 */
int btdestroy(Relation rel);

/**
 * @brief 插入索引条目
 * @param rel 索引 Relation
 * @param values 键值数组
 * @param nkeys 键数量
 * @param heap_ptr 堆元组指针
 * @return 0 成功，-1 失败
 */
int btinsert(Relation rel, const void **values, int nkeys, void *heap_ptr);

/**
 * @brief 删除索引条目
 * @param rel 索引 Relation
 * @param values 键值数组
 * @param nkeys 键数量
 * @param heap_ptr 堆元组指针
 * @return 0 成功，-1 失败
 */
int btdelete(Relation rel, const void **values, int nkeys, void *heap_ptr);

/**
 * @brief 构建索引（批量插入）
 * @param rel 索引 Relation
 * @param tuples 元组数组
 * @param ntuples 元组数
 * @return 0 成功，-1 失败
 */
int btbuild(Relation rel, void **tuples, int ntuples);

/* ============================================================
 * BTree 扫描
 * ============================================================ */

/** BTree 扫描描述符 */
typedef struct BTScanDescData {
    Relation    bt_relation;      /**< 索引 Relation */
    ScanKey     bt_key;           /**< 扫描键 */
    int         bt_nkeys;         /**< 键数量 */
    ScanDirection bt_direction;   /**< 扫描方向 */
    BufferDesc  *bt_curbuf;       /**< 当前缓冲区 */
    uint32_t    bt_curpage;       /**< 当前页面 */
    int         bt_curitem;       /**< 当前条目索引 */
    void        *bt_curtuple;     /**< 当前元组 */
    bool        bt_reorder;       /**< 需要重排序 */
} BTScanDescData;

/** BTree 扫描描述符指针 */
typedef BTScanDescData *BTScanDesc;

/**
 * @brief 开始 BTree 扫描
 * @param rel 索引 Relation
 * @param nkeys 键数量
 * @param key 扫描键
 * @return 扫描描述符
 */
BTScanDesc bt_beginscan(Relation rel, int nkeys, ScanKey key);

/**
 * @brief 开始 BTree 扫描（指定堆 Relation）
 * @param indexrel 索引 Relation
 * @param heaprel 堆 Relation
 * @param nkeys 键数量
 * @param key 扫描键
 * @return 扫描描述符
 */
BTScanDesc bt_index_beginscan(Relation indexrel, Relation heaprel,
                              int nkeys, ScanKey key);

/**
 * @brief 重新扫描
 * @param scan 扫描描述符
 */
void bt_rescan(BTScanDesc scan, ScanKey key);

/**
 * @brief 结束扫描
 * @param scan 扫描描述符
 */
void bt_endscan(BTScanDesc scan);

/**
 * @brief 获取下一个匹配条目
 * @param scan 扫描描述符
 * @return 堆元组指针，到末尾返回 NULL
 */
void *bt_getnext(BTScanDesc scan, ScanDirection direction);

/* ============================================================
 * 键比较
 * ============================================================ */

/**
 * @brief 比较两个键值
 * @param rel 索引 Relation
 * @param key1 键1
 * @param key2 键2
 * @param nkeys 键数量
 * @return <0 key1 < key2, =0 key1 == key2, >0 key1 > key2
 */
int bt_compare(Relation rel, const void *key1, const void *key2, int nkeys);

/**
 * @brief 检查键是否匹配扫描条件
 * @param rel 索引 Relation
 * @param key 键
 * @param nkeys 键数量
 * @param scan_key 扫描键
 * @param nscan_keys 扫描键数量
 * @return true 匹配
 */
bool bt_key_matches(Relation rel, const void *key, int nkeys,
                    ScanKey scan_key, int nscan_keys);

/* ============================================================
 * 页面操作
 * ============================================================ */

/**
 * @brief 初始化 BTree 页面
 * @param page 页面数据
 * @param level 页面层级
 */
void bt_page_init(void *page, uint16_t level);

/**
 * @brief 获取页面头部
 * @param page 页面数据
 * @return 页面头部指针
 */
BTPageHeader bt_page_get_header(void *page);

/**
 * @brief 检查页面是否是叶子页面
 * @param page 页面数据
 * @return true 叶子页面
 */
bool bt_page_is_leaf(void *page);

/**
 * @brief 检查页面是否是根页面
 * @param page 页面数据
 * @return true 根页面
 */
bool bt_page_is_root(void *page);

/**
 * @brief 获取页面空闲空间
 * @param page 页面数据
 * @return 空闲空间字节数
 */
int bt_page_get_free_space(void *page);

/**
 * @brief 定位键在页面中的位置
 * @param page 页面数据
 * @param key 键
 * @param nkeys 键数量
 * @param rel 索引 Relation
 * @return 条目位置（0=最前），-1 未找到
 */
int bt_page_get_item(void *page, const void *key, int nkeys, Relation rel);

/* ============================================================
 * 统计信息
 * ============================================================ */

/** BTree AM 统计信息 */
typedef struct BTREEAMStats_s {
    uint64_t    index_scans;      /**< 索引扫描次数 */
    uint64_t    index_tuples;     /**< 索引元组数 */
    uint64_t    index_pages;      /**< 索引页面数 */
    uint64_t    deletions;        /**< 删除次数 */
    uint64_t    insertions;       /**< 插入次数 */
} BTREEAMStats;

/**
 * @brief 获取 BTree AM 统计信息
 * @param stats 输出统计信息
 */
void btreeam_get_stats(BTREEAMStats *stats);

/**
 * @brief 重置统计信息
 */
void btreeam_reset_stats(void);

/* ============================================================
 * 表达式索引和部分索引
 * ============================================================ */

/**
 * @brief 索引选项
 */
typedef struct IndexOptions_s {
    bool        is_unique;          /**< 是否唯一索引 */
    bool        is_primary;         /**< 是否主键索引 */
    bool        is_partial;         /**< 是否部分索引 */
    bool        is_expression;      /**< 是否表达式索引 */
    const char *where_clause;       /**< 部分索引 WHERE 子句 */
    const char *expression_str;     /**< 表达式字符串（如 "lower(name)"） */
    const void *index_predicate;    /**< 索引谓词（内部表示） */
} IndexOptions;

/**
 * @brief 创建带选项的索引
 * @param rel 索引 Relation
 * @param options 索引选项
 * @return 0 成功，-1 失败
 */
int btcreate_with_options(Relation rel, const IndexOptions *options);

/**
 * @brief 设置索引表达式
 * @param rel 索引 Relation
 * @param expr_str 表达式字符串
 * @return 0 成功，-1 失败
 */
int bt_set_index_expression(Relation rel, const char *expr_str);

/**
 * @brief 获取索引表达式
 * @param rel 索引 Relation
 * @return 表达式字符串（内部存储，不需要释放）
 */
const char *bt_get_index_expression(Relation rel);

/**
 * @brief 设置部分索引谓词
 * @param rel 索引 Relation
 * @param predicate 谓词字符串（如 "status = 'active'"）
 * @return 0 成功，-1 失败
 */
int bt_set_index_predicate(Relation rel, const char *predicate);

/**
 * @brief 获取部分索引谓词
 * @param rel 索引 Relation
 * @return 谓词字符串（内部存储，不需要释放）
 */
const char *bt_get_index_predicate(Relation rel);

/**
 * @brief 检查元组是否满足部分索引谓词
 * @param rel 索引 Relation
 * @param tuple 元组数据
 * @return true 满足，false 不满足
 */
bool bt_check_partial_predicate(Relation rel, const void *tuple);

/**
 * @brief 计算索引表达式的值
 * @param rel 索引 Relation
 * @param tuple 元组数据
 * @param result 输出结果缓冲区
 * @param result_size 缓冲区大小
 * @return 0 成功，-1 失败
 */
int bt_evaluate_expression(Relation rel, const void *tuple,
                           void *result, size_t result_size);

#ifdef __cplusplus
}
#endif

#endif /* DB_BTREEAM_H */
