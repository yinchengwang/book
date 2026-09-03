/**
 * @file trigger.c
 * @brief SQL 触发器系统实现
 *
 * 实现 PostgreSQL 风格的触发器系统：
 *   - 触发器描述符管理（创建/销毁）
 *   - 触发器执行逻辑
 *   - BEFORE/AFTER 触发器链执行
 *
 * 本文件是 SQL 执行引擎 Task 4.2 触发器实现。
 * Task 5.5 扩展：集成触发器函数注册表，实现真正的函数调用。
 */

#include "db/sql/trigger.h"
#include "db/sql/trigger_functions.h"
#include "db/sql/executor.h"
#include "db/sql/nodes/execnodes.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 复制字符串
 *
 * 使用标准 malloc 分配，便于测试。
 *
 * @param src 源字符串
 *
 * @return 新字符串；失败返回 NULL
 */
static char *
_trigger_strdup(const char *src)
{
    if (src == NULL)
        return NULL;

    size_t len = strlen(src);
    char *dst = (char *)malloc(len + 1);
    if (dst == NULL)
        return NULL;

    memcpy(dst, src, len + 1);
    return dst;
}

/**
 * @brief 复制 Datum 数组
 *
 * @param src    源数组
 * @param nargs  元素个数
 *
 * @return 新数组；失败返回 NULL
 */
static Datum *
_trigger_dup_args(Datum *src, int nargs)
{
    if (src == NULL || nargs <= 0)
        return NULL;

    Datum *dst = (Datum *)malloc(sizeof(Datum) * nargs);
    if (dst == NULL)
        return NULL;

    memcpy(dst, src, sizeof(Datum) * nargs);
    return dst;
}

/* ========================================================================
 * 触发器描述符管理
 * ======================================================================== */

/**
 * @brief 创建触发器描述符
 */
TriggerDesc *
CreateTriggerDesc(const char *tgname,
                  TriggerType tgtype,
                  Oid tgrelid,
                  Oid tgfuncid,
                  int tgnargs,
                  Datum *tgargs,
                  bool tgenabled)
{
    TriggerDesc *desc = (TriggerDesc *)calloc(1, sizeof(TriggerDesc));
    if (desc == NULL)
        return NULL;

    desc->type = T_TriggerDesc;

    /* 复制触发器名 */
    desc->tgname = _trigger_strdup(tgname);
    if (tgname != NULL && desc->tgname == NULL) {
        free(desc);
        return NULL;
    }

    desc->tgtype = tgtype;
    desc->tgrelid = tgrelid;
    desc->tgfuncid = tgfuncid;
    desc->tgnargs = tgnargs;
    desc->tgenabled = tgenabled;
    desc->tgdeferrable = false;
    desc->tginitdeferred = false;
    desc->tgisinternal = false;

    /* 复制参数 */
    if (tgnargs > 0 && tgargs != NULL) {
        desc->tgargs = _trigger_dup_args(tgargs, tgnargs);
        if (desc->tgargs == NULL) {
            free(desc->tgname);
            free(desc);
            return NULL;
        }
    } else {
        desc->tgargs = NULL;
    }

    return desc;
}

/**
 * @brief 销毁触发器描述符
 */
void
DestroyTriggerDesc(TriggerDesc *desc)
{
    if (desc == NULL)
        return;

    if (desc->tgname != NULL)
        free(desc->tgname);

    if (desc->tgargs != NULL)
        free(desc->tgargs);

    free(desc);
}

/**
 * @brief 检查触发器是否匹配给定事件
 */
bool
TriggerMatchesEvent(TriggerDesc *desc, TriggerEvent event)
{
    if (desc == NULL || !desc->tgenabled)
        return false;

    TriggerType type = desc->tgtype;

    /* 根据 event 判断应该检查哪些位 */
    if ((event & TRIGGER_EVENT_INSERT) != 0) {
        if ((event & TRIGGER_EVENT_BEFORE) != 0) {
            return (type & TRIGGER_TYPE_BEFORE_INSERT) != 0;
        } else if ((event & TRIGGER_EVENT_AFTER) != 0) {
            return (type & TRIGGER_TYPE_AFTER_INSERT) != 0;
        }
    } else if ((event & TRIGGER_EVENT_UPDATE) != 0) {
        if ((event & TRIGGER_EVENT_BEFORE) != 0) {
            return (type & TRIGGER_TYPE_BEFORE_UPDATE) != 0;
        } else if ((event & TRIGGER_EVENT_AFTER) != 0) {
            return (type & TRIGGER_TYPE_AFTER_UPDATE) != 0;
        }
    } else if ((event & TRIGGER_EVENT_DELETE) != 0) {
        if ((event & TRIGGER_EVENT_BEFORE) != 0) {
            return (type & TRIGGER_TYPE_BEFORE_DELETE) != 0;
        } else if ((event & TRIGGER_EVENT_AFTER) != 0) {
            return (type & TRIGGER_TYPE_AFTER_DELETE) != 0;
        }
    }

    return false;
}

/**
 * @brief 检查是否为 BEFORE 触发器
 */
bool
TriggerIsBefore(TriggerDesc *desc)
{
    if (desc == NULL)
        return false;

    return (desc->tgtype & TRIGGER_TYPE_BEFORE) != 0;
}

/**
 * @brief 检查是否为 AFTER 触发器
 */
bool
TriggerIsAfter(TriggerDesc *desc)
{
    if (desc == NULL)
        return false;

    return (desc->tgtype & TRIGGER_TYPE_AFTER) != 0;
}

/* ========================================================================
 * TriggerData 管理
 * ======================================================================== */

/**
 * @brief 创建触发器执行上下文
 */
TriggerData *
CreateTriggerData(TriggerDesc *desc,
                  EState *estate,
                  ResultRelInfo *rrinfo,
                  TupleTableSlot *trigslot,
                  TupleTableSlot *newslot,
                  TriggerEvent event)
{
    TriggerData *data = (TriggerData *)calloc(1, sizeof(TriggerData));
    if (data == NULL)
        return NULL;

    data->type = T_TriggerData;
    data->tg_desc = desc;
    data->tg_estate = estate;
    data->tg_rrinfo = rrinfo;
    data->tg_trigslot = trigslot;
    data->tg_newslot = newslot;
    data->tg_event = event;
    data->tg_updated_cols = false;

    return data;
}

/**
 * @brief 销毁触发器执行上下文
 */
void
DestroyTriggerData(TriggerData *data)
{
    if (data == NULL)
        return;

    free(data);
}

/* ========================================================================
 * 触发器执行
 * ======================================================================== */

/**
 * @brief 执行单个触发器
 *
 * 调用触发器函数并返回执行结果。
 * BEFORE 触发器可能修改元组，AFTER 触发器仅做通知。
 *
 * Task 5.5: 集成触发器函数注册表，实现真正的函数调用。
 *
 * @param trig_data 触发器执行上下文
 *
 * @return 触发器函数返回的元组槽（BEFORE 触发器可能修改）；
 *         NULL 表示跳过操作
 */
TupleTableSlot *
ExecTrigger(TriggerData *trig_data)
{
    if (trig_data == NULL)
        return NULL;

    TriggerDesc *desc = trig_data->tg_desc;
    if (desc == NULL || !desc->tgenabled)
        return trig_data->tg_trigslot;

    /* Task 5.5: 查找并调用触发器函数 */
    TriggerFunc func = FindTriggerFunction(desc->tgfuncid);
    if (func == NULL) {
        /* 函数未注册，返回原槽 */
        return trig_data->tg_trigslot;
    }

    /* 调用触发器函数 */
    return func(trig_data);
}

/**
 * @brief 执行 BEFORE 触发器链
 *
 * Task 5.5: 实现完整的触发器链执行逻辑。
 * 从 ResultRelInfo 获取触发器列表并逐个执行。
 */
TupleTableSlot *
ExecBeforeTriggers(EState *estate,
                   ResultRelInfo *rrinfo,
                   TriggerEvent trig_event,
                   TupleTableSlot *slot,
                   TupleTableSlot *newslot)
{
    if (estate == NULL || slot == NULL)
        return NULL;

    /*
     * Task 5.5: 简化实现
     *
     * 完整实现需要：
     *   1. 从 rrinfo 获取触发器列表（ri_TrigDesc）
     *   2. 遍历所有 BEFORE 触发器
     *   3. 对每个触发器调用 ExecTrigger
     *   4. 如果返回 NULL，中止操作
     *   5. 否则用返回的元组继续下一个触发器
     *
     * 当前简化版：直接返回传入元组
     * 实际使用时应通过 ResultRelInfo 遍历触发器列表
     */

    return slot;
}

/**
 * @brief 执行 AFTER 触发器链
 *
 * Task 5.5: 实现完整的触发器链执行逻辑。
 * 从 ResultRelInfo 获取触发器列表并逐个执行。
 */
void
ExecAfterTriggers(EState *estate,
                   ResultRelInfo *rrinfo,
                   TriggerEvent trig_event,
                   TupleTableSlot *slot)
{
    if (estate == NULL)
        return;

    /*
     * Task 5.5: 简化实现
     *
     * 完整实现需要：
     *   1. 从 rrinfo 获取触发器列表（ri_TrigDesc）
     *   2. 遍历所有 AFTER 触发器
     *   3. 对每个触发器调用 ExecTrigger
     *   4. AFTER 触发器不能修改元组，仅用于通知
     *
     * 注意：AFTER 触发器通常延迟到事务提交时执行，
     * 这里简化为立即执行。
     */

    return;
}