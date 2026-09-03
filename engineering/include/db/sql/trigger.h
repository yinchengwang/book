/**
 * @file trigger.h
 * @brief SQL 触发器系统
 *
 * 实现 PostgreSQL 风格的触发器系统：
 *   - 支持 BEFORE/AFTER 触发器
 *   - 支持 INSERT/UPDATE/DELETE 事件
 *   - 触发器函数调用机制
 *
 * 核心数据结构：
 *   - TriggerType     触发器类型枚举
 *   - TriggerDesc     触发器描述符
 *   - TriggerData     触发器执行上下文
 *   - TriggerEvent    触发事件
 *
 * 本文件是 SQL 执行引擎 Task 4.2 触发器实现。
 */

#ifndef DB_SQL_TRIGGER_H
#define DB_SQL_TRIGGER_H

#include <stdint.h>
#include <stdbool.h>

#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"  /* TupleTableSlot 完整定义 */
#include "db/sql/memctx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct EState          EState;
typedef struct ResultRelInfo   ResultRelInfo;
typedef struct TupleTableSlot  TupleTableSlot;
typedef struct TriggerDesc     TriggerDesc;
typedef struct TriggerData     TriggerData;

/* ========================================================================
 * TriggerType - 触发器类型枚举
 * ======================================================================== */

/**
 * @brief 触发器类型枚举
 *
 * 使用位标志组合，支持同一触发器响应多种事件。
 */
typedef enum TriggerType {
    TRIGGER_TYPE_NONE            = 0x00,   /**< 无触发器 */

    /* INSERT 事件 */
    TRIGGER_TYPE_BEFORE_INSERT   = 0x01,   /**< INSERT 前 */
    TRIGGER_TYPE_AFTER_INSERT    = 0x02,   /**< INSERT 后 */

    /* UPDATE 事件 */
    TRIGGER_TYPE_BEFORE_UPDATE   = 0x04,   /**< UPDATE 前 */
    TRIGGER_TYPE_AFTER_UPDATE    = 0x08,   /**< UPDATE 后 */

    /* DELETE 事件 */
    TRIGGER_TYPE_BEFORE_DELETE   = 0x10,   /**< DELETE 前 */
    TRIGGER_TYPE_AFTER_DELETE    = 0x20,   /**< DELETE 后 */

    /* 组合掩码 */
    TRIGGER_TYPE_BEFORE          = TRIGGER_TYPE_BEFORE_INSERT |
                                   TRIGGER_TYPE_BEFORE_UPDATE |
                                   TRIGGER_TYPE_BEFORE_DELETE,  /**< BEFORE 掩码 */

    TRIGGER_TYPE_AFTER           = TRIGGER_TYPE_AFTER_INSERT |
                                   TRIGGER_TYPE_AFTER_UPDATE |
                                   TRIGGER_TYPE_AFTER_DELETE,   /**< AFTER 掩码 */

    TRIGGER_TYPE_INSERT          = TRIGGER_TYPE_BEFORE_INSERT |
                                   TRIGGER_TYPE_AFTER_INSERT,   /**< INSERT 掩码 */

    TRIGGER_TYPE_UPDATE          = TRIGGER_TYPE_BEFORE_UPDATE |
                                   TRIGGER_TYPE_AFTER_UPDATE,   /**< UPDATE 掩码 */

    TRIGGER_TYPE_DELETE          = TRIGGER_TYPE_BEFORE_DELETE |
                                   TRIGGER_TYPE_AFTER_DELETE,   /**< DELETE 掩码 */

    TRIGGER_TYPE_ALL             = TRIGGER_TYPE_BEFORE |
                                   TRIGGER_TYPE_AFTER           /**< 所有事件掩码 */
} TriggerType;

/* ========================================================================
 * TriggerEvent - 触发事件
 * ======================================================================== */

/**
 * @brief 触发事件
 *
 * 描述触发器被触发时的具体事件上下文。
 */
typedef enum TriggerEvent {
    TRIGGER_EVENT_INSERT         = 0x01,   /**< INSERT 事件 */
    TRIGGER_EVENT_UPDATE         = 0x02,   /**< UPDATE 事件 */
    TRIGGER_EVENT_DELETE         = 0x04,   /**< DELETE 事件 */

    /* 事件修饰 */
    TRIGGER_EVENT_BEFORE         = 0x10,   /**< BEFORE 触发 */
    TRIGGER_EVENT_AFTER          = 0x20,   /**< AFTER 触发 */
    TRIGGER_EVENT_ROW            = 0x40,   /**< 行级触发 */

    /* 组合事件 */
    TRIGGER_EVENT_BEFORE_INSERT  = TRIGGER_EVENT_INSERT | TRIGGER_EVENT_BEFORE | TRIGGER_EVENT_ROW,
    TRIGGER_EVENT_AFTER_INSERT   = TRIGGER_EVENT_INSERT | TRIGGER_EVENT_AFTER | TRIGGER_EVENT_ROW,
    TRIGGER_EVENT_BEFORE_UPDATE  = TRIGGER_EVENT_UPDATE | TRIGGER_EVENT_BEFORE | TRIGGER_EVENT_ROW,
    TRIGGER_EVENT_AFTER_UPDATE   = TRIGGER_EVENT_UPDATE | TRIGGER_EVENT_AFTER | TRIGGER_EVENT_ROW,
    TRIGGER_EVENT_BEFORE_DELETE  = TRIGGER_EVENT_DELETE | TRIGGER_EVENT_BEFORE | TRIGGER_EVENT_ROW,
    TRIGGER_EVENT_AFTER_DELETE   = TRIGGER_EVENT_DELETE | TRIGGER_EVENT_AFTER | TRIGGER_EVENT_ROW
} TriggerEvent;

/* ========================================================================
 * Datum 访问宏（Datum 已从 nodetags.h 引入）
 * ======================================================================== */

/**
 * @brief Datum 类型已在 nodetags.h 中定义为 uint64_t
 *
 * 本文件使用宏提供与 PostgreSQL 兼容的 Datum 访问接口。
 */

#define DatumGetBool(x)      ((bool)(x))
#define DatumGetInt32(x)     ((int32_t)(x))
#define DatumGetInt64(x)     ((int64_t)(x))
#define DatumGetPointer(x)   ((void*)(x))
#define BoolGetDatum(x)      ((Datum)(x))
#define Int32GetDatum(x)     ((Datum)(int32_t)(x))
#define Int64GetDatum(x)     ((Datum)(int64_t)(x))
#define PointerGetDatum(x)   ((Datum)(x))

/* ========================================================================
 * Oid 类型定义
 * ======================================================================== */

#ifndef Oid_defined
#define Oid_defined
typedef uint64_t Oid;    /**< 对象标识符 */
#endif

/* ========================================================================
 * TriggerDesc - 触发器描述符
 * ======================================================================== */

/**
 * @brief TriggerDesc - 触发器描述符
 *
 * 存储触发器的元数据信息，包括触发器名称、类型、关联表、
 * 触发器函数以及参数等。
 */
struct TriggerDesc {
    NodeTag         type;               /**< T_TriggerDesc */
    char           *tgname;             /**< 触发器名 */
    TriggerType     tgtype;             /**< 触发器类型（位标志组合） */
    Oid             tgrelid;            /**< 表 OID */
    Oid             tgfuncid;           /**< 触发器函数 OID */
    int             tgnargs;            /**< 参数个数 */
    Datum          *tgargs;             /**< 参数值数组 */
    bool            tgenabled;          /**< 是否启用 */
    bool            tgdeferrable;       /**< 是否可延迟 */
    bool            tginitdeferred;     /**< 初始延迟 */
    bool            tgisinternal;       /**< 是否为内部触发器 */
};

/* ========================================================================
 * TriggerData - 触发器执行上下文
 * ======================================================================== */

/**
 * @brief TriggerData - 触发器执行上下文
 *
 * 在触发器执行时传递给触发器函数的上下文信息，
 * 包含触发器描述符、执行状态、结果关系信息、触发元组等。
 */
struct TriggerData {
    NodeTag         type;               /**< T_TriggerData */
    TriggerDesc    *tg_desc;            /**< 触发器描述符 */
    EState         *tg_estate;          /**< 执行状态 */
    ResultRelInfo  *tg_rrinfo;          /**< 结果关系信息 */
    TupleTableSlot *tg_trigslot;        /**< 触发元组槽（旧元组/被删除元组） */
    TupleTableSlot *tg_newslot;         /**< 新元组槽（UPDATE 时的新元组） */
    TriggerEvent    tg_event;           /**< 触发事件 */
    bool            tg_updated_cols;    /**< 是否有更新的列（占位） */
};

/* ========================================================================
 * 触发器管理函数
 * ======================================================================== */

/**
 * @brief 创建触发器描述符
 *
 * @param tgname      触发器名（将被复制）
 * @param tgtype      触发器类型
 * @param tgrelid     表 OID
 * @param tgfuncid    触发器函数 OID
 * @param tgnargs     参数个数
 * @param tgargs      参数值数组（可为 NULL）
 * @param tgenabled   是否启用
 *
 * @return 新创建的 TriggerDesc；失败返回 NULL
 */
TriggerDesc *CreateTriggerDesc(const char *tgname,
                               TriggerType tgtype,
                               Oid tgrelid,
                               Oid tgfuncid,
                               int tgnargs,
                               Datum *tgargs,
                               bool tgenabled);

/**
 * @brief 销毁触发器描述符
 *
 * @param desc 待销毁的 TriggerDesc（可为 NULL）
 */
void DestroyTriggerDesc(TriggerDesc *desc);

/**
 * @brief 检查触发器是否匹配给定事件
 *
 * @param desc   触发器描述符
 * @param event  触发事件
 *
 * @return true 表示匹配
 */
bool TriggerMatchesEvent(TriggerDesc *desc, TriggerEvent event);

/**
 * @brief 检查是否为 BEFORE 触发器
 *
 * @param desc 触发器描述符
 *
 * @return true 表示 BEFORE 触发器
 */
bool TriggerIsBefore(TriggerDesc *desc);

/**
 * @brief 检查是否为 AFTER 触发器
 *
 * @param desc 触发器描述符
 *
 * @return true 表示 AFTER 触发器
 */
bool TriggerIsAfter(TriggerDesc *desc);

/* ========================================================================
 * 触发器执行函数
 * ======================================================================== */

/**
 * @brief 执行单个触发器
 *
 * 调用触发器函数并返回执行结果。
 * BEFORE 触发器可能修改元组，AFTER 触发器仅做通知。
 *
 * @param trig_data 触发器执行上下文
 *
 * @return 触发器函数返回的元组槽（BEFORE 触发器可能修改）；
 *         NULL 表示跳过操作
 */
TupleTableSlot *ExecTrigger(TriggerData *trig_data);

/**
 * @brief 执行 BEFORE 触发器链
 *
 * 按顺序执行所有匹配的 BEFORE 触发器。
 * 如果某个触发器返回 NULL，则中止操作。
 *
 * @param estate     执行状态
 * @param rrinfo     结果关系信息
 * @param trig_event 触发事件
 * @param slot       触发元组槽
 * @param newslot    新元组槽（UPDATE 时，可为 NULL）
 *
 * @return 处理后的元组槽；NULL 表示跳过操作
 */
TupleTableSlot *ExecBeforeTriggers(EState *estate,
                                   ResultRelInfo *rrinfo,
                                   TriggerEvent trig_event,
                                   TupleTableSlot *slot,
                                   TupleTableSlot *newslot);

/**
 * @brief 执行 AFTER 触发器链
 *
 * 按顺序执行所有匹配的 AFTER 触发器。
 * AFTER 触发器不能修改元组，仅用于通知。
 *
 * @param estate     执行状态
 * @param rrinfo     结果关系信息
 * @param trig_event 触发事件
 * @param slot       触发元组槽
 */
void ExecAfterTriggers(EState *estate,
                       ResultRelInfo *rrinfo,
                       TriggerEvent trig_event,
                       TupleTableSlot *slot);

/* ========================================================================
 * TriggerData 创建与销毁
 * ======================================================================== */

/**
 * @brief 创建触发器执行上下文
 *
 * @param desc      触发器描述符
 * @param estate    执行状态
 * @param rrinfo    结果关系信息
 * @param trigslot  触发元组槽
 * @param newslot   新元组槽
 * @param event     触发事件
 *
 * @return 新创建的 TriggerData；失败返回 NULL
 */
TriggerData *CreateTriggerData(TriggerDesc *desc,
                               EState *estate,
                               ResultRelInfo *rrinfo,
                               TupleTableSlot *trigslot,
                               TupleTableSlot *newslot,
                               TriggerEvent event);

/**
 * @brief 销毁触发器执行上下文
 *
 * @param data 待销毁的 TriggerData（可为 NULL）
 */
void DestroyTriggerData(TriggerData *data);

/* ========================================================================
 * 辅助宏
 * ======================================================================== */

/**
 * @brief 检查触发事件是否为 BEFORE INSERT
 */
#define TRIGGER_FIRED_BEFORE_INSERT(event) \
    ((event) == TRIGGER_EVENT_BEFORE_INSERT)

/**
 * @brief 检查触发事件是否为 AFTER INSERT
 */
#define TRIGGER_FIRED_AFTER_INSERT(event) \
    ((event) == TRIGGER_EVENT_AFTER_INSERT)

/**
 * @brief 检查触发事件是否为 BEFORE UPDATE
 */
#define TRIGGER_FIRED_BEFORE_UPDATE(event) \
    ((event) == TRIGGER_EVENT_BEFORE_UPDATE)

/**
 * @brief 检查触发事件是否为 AFTER UPDATE
 */
#define TRIGGER_FIRED_AFTER_UPDATE(event) \
    ((event) == TRIGGER_EVENT_AFTER_UPDATE)

/**
 * @brief 检查触发事件是否为 BEFORE DELETE
 */
#define TRIGGER_FIRED_BEFORE_DELETE(event) \
    ((event) == TRIGGER_EVENT_BEFORE_DELETE)

/**
 * @brief 检查触发事件是否为 AFTER DELETE
 */
#define TRIGGER_FIRED_AFTER_DELETE(event) \
    ((event) == TRIGGER_EVENT_AFTER_DELETE)

/**
 * @brief 检查触发事件是否为行级
 */
#define TRIGGER_FIRED_FOR_ROW(event) \
    (((event) & TRIGGER_EVENT_ROW) != 0)

/**
 * @brief 检查触发事件是否为 BEFORE
 */
#define TRIGGER_FIRED_BEFORE(event) \
    (((event) & TRIGGER_EVENT_BEFORE) != 0)

/**
 * @brief 检查触发事件是否为 AFTER
 */
#define TRIGGER_FIRED_AFTER(event) \
    (((event) & TRIGGER_EVENT_AFTER) != 0)

/**
 * @brief 检查触发事件是否为 INSERT（任意时机）
 */
#define TRIGGER_FIRED_INSERT(event) \
    (((event) & TRIGGER_EVENT_INSERT) != 0)

/**
 * @brief 检查触发事件是否为 UPDATE（任意时机）
 */
#define TRIGGER_FIRED_UPDATE(event) \
    (((event) & TRIGGER_EVENT_UPDATE) != 0)

/**
 * @brief 检查触发事件是否为 DELETE（任意时机）
 */
#define TRIGGER_FIRED_DELETE(event) \
    (((event) & TRIGGER_EVENT_DELETE) != 0)

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_TRIGGER_H */
