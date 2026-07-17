/**
 * @file rel.h
 * @brief Relation（关系）和 Access Method 公共接口
 *
 * Relation 是 PostgreSQL 风格存储架构的核心抽象，
 * 表示一个表、索引或其他可访问对象。
 */
#ifndef DB_REL_H
#define DB_REL_H

#include "db/catalog.h"
#include "db/buf.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** Relation 打开模式 */
typedef enum RelOpenMode_e {
    RELMODE_READ = 0,      /**< 只读模式 */
    RELMODE_WRITE = 1      /**< 读写模式 */
} RelOpenMode;

/** Relation 类型 */
typedef enum RelKind_e {
    RELKIND_RELATION = 'r',    /**< 普通表 */
    RELKIND_INDEX = 'i',       /**< 索引 */
    RELKIND_SEQUENCE = 'S',    /**< 序列 */
    RELKIND_VIEW = 'v',        /**< 视图 */
    RELKIND_COMPOSITE_TYPE = 'c' /**< 复合类型 */
} RelKind;

/** 访问方法类型 */
typedef enum AccessMethodType_e {
    AM_HEAP = 0,       /**< Heap 访问方法（表） */
    AM_BTREE = 1,      /**< BTree 访问方法 */
    AM_HASH = 2,       /**< Hash 访问方法 */
    AM_GIST = 3,       /**< GiST 访问方法 */
    AM_GIN = 4,        /**< GIN 访问方法 */
    AM_BRIN = 5        /**< BRIN 访问方法 */
} AccessMethodType;

/** Relation 打开标志 */
#define REL_OPEN_READONLY   0x01
#define REL_OPEN_READWRITE  0x02
#define REL_OPEN_UPDATING   0x04  /* 用于行锁 */

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct RelationData *Relation;
typedef struct TupleDescData *TupleDesc;
typedef struct ScanKeyData *ScanKey;
typedef struct IndexScanDescData *IndexScanDesc;

/* ============================================================
 * TupleDesc（行描述符）
 * ============================================================ */

/**
 * @brief TupleDesc 数据结构
 *
 * 描述 Relation 的列结构
 */
struct TupleDescData {
    int         natts;          /**< 列数 */
    int         tdtypeid;       /**< 行类型 OID */
    int         tdtypmod;       /**< 类型修饰符 */
    int         tdenv;          /**< 环境 ID */
    bool        tdhasoid;       /**< 是否有 OID 列 */

    /* 列信息数组 */
    struct {
        Oid         atttypid;       /**< 列类型 OID */
        int32_t     atttypmod;      /**< 类型修饰符 */
        int16_t     attlen;         /**< 类型长度 */
        int16_t     attalign;       /**< 对齐要求 */
        int16_t     attbyval;       /**< 是否按值传递 */
        char        attstorage;     /**< 存储策略 */
        bool        attnotnull;     /**< NOT NULL */
        bool        atthasdef;      /**< 有默认值 */
        char        attname[NAMEDATALEN]; /**< 列名 */
    } *attrs;
};

/**
 * @brief 创建 TupleDesc
 * @param natts 列数
 * @return TupleDesc
 */
TupleDesc CreateTupleDesc(int natts);

/**
 * @brief 复制 TupleDesc
 * @param src 源描述符
 * @return 新的 TupleDesc
 */
TupleDesc CreateTupleDescCopy(TupleDesc src);

/**
 * @brief 释放 TupleDesc
 * @param tdesc TupleDesc
 */
void FreeTupleDesc(TupleDesc tdesc);

/**
 * @brief 获取 TupleDesc 中的列数
 * @param tdesc TupleDesc
 * @return 列数
 */
int TupleDescNatts(TupleDesc tdesc);

/**
 * @brief 获取列信息
 * @param tdesc TupleDesc
 * @param attnum 列号（从 1 开始）
 * @return 列信息指针
 */
void *TupleDescAttr(TupleDesc tdesc, int attnum);

/* ============================================================
 * ScanKey（扫描键）
 * ============================================================ */

/**
 * @brief ScanKey 操作符
 */
typedef enum ScanKeyOp_e {
    SCAN_KEY_EQ = 0,     /**< 等于 */
    SCAN_KEY_LT = 1,     /**< 小于 */
    SCAN_KEY_LE = 2,     /**< 小于等于 */
    SCAN_KEY_GT = 3,     /**< 大于 */
    SCAN_KEY_GE = 4,     /**< 大于等于 */
    SCAN_KEY_NE = 5,     /**< 不等于 */
    SCAN_KEY_SEARCH = 6, /**< 搜索（前缀匹配） */
    SCAN_KEY_NSEARCH = 7 /**< 逆向搜索 */
} ScanKeyOp;

/**
 * @brief ScanKey 数据
 */
struct ScanKeyData {
    int         sk_attno;       /**< 列号（从 1 开始） */
    ScanKeyOp   sk_op;          /**< 操作符 */
    Oid         sk_procedure;   /**< 比较函数 OID */
    void        *sk_argument;   /**< 比较值 */
    size_t      sk_arglen;      /**< 比较值长度 */
};

/**
 * @brief 初始化 ScanKey
 * @param key ScanKey
 * @param attno 列号
 * @param op 操作符
 * @param argument 比较值
 */
void ScanKeyInit(ScanKey key, int attno, ScanKeyOp op, void *argument);

/**
 * @brief 初始化带长度的 ScanKey
 * @param key ScanKey
 * @param attno 列号
 * @param op 操作符
 * @param arglen 比较值长度
 * @param argument 比较值
 */
void ScanKeyInitWithInfo(ScanKey key, int attno, ScanKeyOp op,
                         size_t arglen, void *argument);

/* ============================================================
 * Relation 数据结构
 * ============================================================ */

/**
 * @brief Relation 数据结构
 *
 * 代表一个打开的 Relation（表、索引等）
 */
struct RelationData {
    Oid         rd_id;              /**< Relation OID */
    Oid         rd_relid;           /**< pg_class OID */
    RelKind     rd_relkind;         /**< Relation 类型 */
    AccessMethodType rd_am;         /**< 访问方法类型 */

    /* 物理信息 */
    Oid         rd_relfilenode;     /**< 物理文件节点 */
    Oid         rd_tablespace;      /**< 表空间 */
    Oid         rd_backend;         /**< 后端 ID */
    int         rd_nblocks;         /**< 总块数 */

    /* 锁信息 */
    int         rd_lockmode;        /**< 当前锁模式 */
    bool        rd_islocal;         /**< 是否本地 Relation */

    /* 元数据 */
    TupleDesc   rd_att;             /**< 行描述符 */
    Oid         rd_colversion[64];  /**< 列版本号（用于 DDL 检测） */

    /* AM 特定数据 */
    void        *rd_amstate;        /**< AM 状态 */
    void        *rd_fd;             /**< AM 文件描述符 */

    /* Buffer Pool */
    void        *rd_bufferPool;     /**< Buffer Pool 指针 */

    /* 引用计数 */
    int         rd_refcnt;          /**< 引用计数 */
};

/* ============================================================
 * Relation 操作
 * ============================================================ */

/**
 * @brief 打开 Relation
 * @param relid Relation OID
 * @param mode 打开模式
 * @return Relation 句柄，失败返回 NULL
 */
Relation relation_open(Oid relid, int mode);

/**
 * @brief 打开指定 relfilenode 的 Relation
 * @param relfilenode 物理文件节点
 * @param mode 打开模式
 * @return Relation 句柄
 */
Relation relation_opennode(uint32_t relfilenode, int mode);

/**
 * @brief 关闭 Relation
 * @param rel Relation
 */
void relation_close(Relation rel, int mode);

/**
 * @brief 创建 Relation
 * @param relid Relation OID（由 Catalog 分配）
 * @param tdesc 行描述符
 * @param relkind Relation 类型
 * @param amtype 访问方法类型
 * @return 0 成功，-1 失败
 */
int relation_create(Oid relid, TupleDesc tdesc, RelKind relkind,
                    AccessMethodType amtype);

/**
 * @brief 删除 Relation
 * @param relid Relation OID
 * @return 0 成功，-1 失败
 */
int relation_drop(Oid relid);

/**
 * @brief 获取 Relation 的行描述符
 * @param rel Relation
 * @return TupleDesc
 */
TupleDesc relation_getdesc(Relation rel);

/**
 * @brief 获取 Relation 的列数
 * @param rel Relation
 * @return 列数
 */
int relation_getnatts(Relation rel);

/**
 * @brief 获取 Relation 的块数
 * @param rel Relation
 * @return 块数
 */
BlockNumber relation_getnblocks(Relation rel);

/**
 * @brief 获取 Relation 的 relfilenode
 * @param rel Relation
 * @return relfilenode
 */
uint32_t relation_getfilenode(Relation rel);

/**
 * @brief 获取 Relation 的访问方法
 * @param rel Relation
 * @return AM 类型
 */
AccessMethodType relation_getam(Relation rel);

/* ============================================================
 * Relation 扫描
 * ============================================================ */

/** 扫描方向 */
typedef enum ScanDirection_e {
    ForwardScanDirection = 0,   /**< 正向扫描 */
    BackwardScanDirection = 1   /**< 反向扫描 */
} ScanDirection;

/**
 * @brief 扫描描述符
 */
typedef struct TableScanDescData {
    Relation    rs_rd;              /**< Relation */
    BlockNumber rs_startblock;      /**< 起始块 */
    BlockNumber rs_numblocks;       /**< 块数 */
    BlockNumber rs_cblock;          /**< 当前块 */
    int         rs_cindex;          /**< 当前索引 */
    BufferDesc  *rs_cbuf;           /**< 当前 Buffer */
    void        *rs_ctbuf;          /**< 当前元组 */
    ScanKey     rs_key;             /**< 扫描键 */
    int         rs_nkeys;           /**< 扫描键数量 */
    ScanDirection rs_direction;     /**< 扫描方向 */
} TableScanDescData;

/** 表扫描描述符 */
typedef TableScanDescData *TableScanDesc;

/**
 * @brief 开始表扫描
 * @param rel Relation
 * @param nkeys 扫描键数量
 * @param key 扫描键数组
 * @return 扫描描述符
 */
TableScanDesc table_beginscan(Relation rel, int nkeys, ScanKey key);

/**
 * @brief 开始关系扫描
 * @param rel Relation
 * @return 扫描描述符
 */
TableScanDesc table_beginscan_all(Relation rel);

/**
 * @brief 结束扫描
 * @param scan 扫描描述符
 */
void table_endscan(TableScanDesc scan);

/**
 * @brief 获取下一个元组
 * @param scan 扫描描述符
 * @return 元组指针，到达末尾返回 NULL
 */
void *table_getnext(TableScanDesc scan);

/**
 * @brief 获取当前元组
 * @param scan 扫描描述符
 * @return 元组指针
 */
void *table_getcurr(TableScanDesc scan);

/* ============================================================
 * 页面操作（使用 Heap AM）
 * ============================================================ */

/* 注意：heap_insert, heap_update, heap_delete 等函数已移至 heapam.h */

/** 无效块号 */
#define InvalidBlockNumber ((BlockNumber)0)

/* ============================================================
 * 索引扫描
 * ============================================================ */

/**
 * @brief 索引扫描描述符
 */
struct IndexScanDescData {
    Relation    idx_relation;        /**< 索引 Relation */
    Relation    heap_relation;       /**< 堆 Relation */
    ScanKey     key;                 /**< 扫描键 */
    int         nkeys;               /**< 键数量 */
    ScanDirection direction;         /**< 扫描方向 */
    Oid         *opclasses;          /**< 操作符类 */
};

/**
 * @brief 开始索引扫描
 * @param rel 索引 Relation
 * @param nkeys 键数量
 * @param key 扫描键
 * @return 扫描描述符
 */
IndexScanDesc index_beginscan(Relation rel, int nkeys, ScanKey key);

/**
 * @brief 开始索引扫描（指定堆 Relation）
 * @param indexrel 索引 Relation
 * @param heaprel 堆 Relation
 * @param nkeys 键数量
 * @param key 扫描键
 * @return 扫描描述符
 */
IndexScanDesc index_beginscan_heapscan(Relation indexrel, Relation heaprel,
                                       int nkeys, ScanKey key);

/**
 * @brief 结束索引扫描
 * @param scan 扫描描述符
 */
void index_endscan(IndexScanDesc scan);

/**
 * @brief 获取下一个索引条目
 * @param scan 扫描描述符
 * @return 元组指针
 */
void *index_getnext(IndexScanDesc scan);

/* ============================================================
 * 初始化
 * ============================================================ */

/**
 * @brief 初始化 Relation 子系统
 * @return 0 成功
 */
int rel_init(void);

/**
 * @brief 关闭 Relation 子系统
 */
void rel_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_REL_H */
