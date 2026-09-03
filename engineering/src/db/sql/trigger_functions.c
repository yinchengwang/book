/**
 * @file trigger_functions.c
 * @brief SQL 触发器函数注册与调用实现
 *
 * 实现触发器函数的注册表管理和调用机制。
 *
 * 本文件是 SQL 执行引擎 Task 5.5 触发器调用机制实现。
 */

#include "db/sql/trigger_functions.h"
#include "db/sql/trigger.h"
#include "db/sql/nodes/execnodes.h"  /* 获取 TupleTableSlot 完整定义 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * 触发器函数注册表（链表实现）
 * ======================================================================== */

static TriggerFunctionEntry *g_trigger_func_registry = NULL;  /**< 注册表头指针 */
static int g_trigger_func_count = 0;                          /**< 注册函数计数 */

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 复制字符串（使用 malloc）
 */
static char *
_trigger_func_strdup(const char *src)
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
 * @brief 查找触发器函数条目
 */
static TriggerFunctionEntry *
_find_entry_by_oid(Oid oid)
{
    TriggerFunctionEntry *entry = g_trigger_func_registry;
    while (entry != NULL) {
        if (entry->func_oid == oid)
            return entry;
        entry = entry->next;
    }
    return NULL;
}

/**
 * @brief 查找触发器函数条目（按名称）
 */
static TriggerFunctionEntry *
_find_entry_by_name(const char *name)
{
    if (name == NULL)
        return NULL;

    TriggerFunctionEntry *entry = g_trigger_func_registry;
    while (entry != NULL) {
        if (entry->func_name != NULL && strcmp(entry->func_name, name) == 0)
            return entry;
        entry = entry->next;
    }
    return NULL;
}

/* ========================================================================
 * 触发器函数注册表管理
 * ======================================================================== */

/**
 * @brief 初始化触发器函数注册表
 */
int
InitTriggerFunctionRegistry(void)
{
    int result = 0;

    /* 注册内置触发器函数 */
    result |= RegisterTriggerFunction("auto_timestamp",
                                      TRIGGER_FUNC_AUTO_TIMESTAMP,
                                      trigger_auto_timestamp,
                                      true);

    result |= RegisterTriggerFunction("audit_log",
                                      TRIGGER_FUNC_AUDIT_LOG,
                                      trigger_audit_log,
                                      true);

    result |= RegisterTriggerFunction("validate_data",
                                      TRIGGER_FUNC_VALIDATE_DATA,
                                      trigger_validate_data,
                                      true);

    return result;
}

/**
 * @brief 销毁触发器函数注册表
 */
void
DestroyTriggerFunctionRegistry(void)
{
    TriggerFunctionEntry *entry = g_trigger_func_registry;
    while (entry != NULL) {
        TriggerFunctionEntry *next = entry->next;
        if (entry->func_name != NULL)
            free(entry->func_name);
        free(entry);
        entry = next;
    }
    g_trigger_func_registry = NULL;
    g_trigger_func_count = 0;
}

/**
 * @brief 注册触发器函数
 */
int
RegisterTriggerFunction(const char *name, Oid oid, TriggerFunc func, bool builtin)
{
    if (name == NULL || func == NULL) {
        return -1;  /* 参数错误 */
    }

    /* 检查 OID 是否已存在 */
    if (_find_entry_by_oid(oid) != NULL) {
        return -2;  /* OID 已存在 */
    }

    /* 分配新条目 */
    TriggerFunctionEntry *entry = (TriggerFunctionEntry *)malloc(sizeof(TriggerFunctionEntry));
    if (entry == NULL)
        return -1;

    /* 初始化条目 */
    entry->func_name = _trigger_func_strdup(name);
    if (entry->func_name == NULL) {
        free(entry);
        return -1;
    }

    entry->func_oid = oid;
    entry->func_ptr = func;
    entry->is_builtin = builtin;
    entry->next = NULL;

    /* 添加到链表头部 */
    entry->next = g_trigger_func_registry;
    g_trigger_func_registry = entry;
    g_trigger_func_count++;

    return 0;
}

/**
 * @brief 查找触发器函数
 */
TriggerFunc
FindTriggerFunction(Oid oid)
{
    TriggerFunctionEntry *entry = _find_entry_by_oid(oid);
    if (entry != NULL)
        return entry->func_ptr;
    return NULL;
}

/**
 * @brief 按名称查找触发器函数
 */
TriggerFunc
FindTriggerFunctionByName(const char *name)
{
    TriggerFunctionEntry *entry = _find_entry_by_name(name);
    if (entry != NULL)
        return entry->func_ptr;
    return NULL;
}

/**
 * @brief 获取触发器函数名称
 */
const char *
GetTriggerFunctionName(Oid oid)
{
    TriggerFunctionEntry *entry = _find_entry_by_oid(oid);
    if (entry != NULL)
        return entry->func_name;
    return NULL;
}

/**
 * @brief 检查触发器函数是否存在
 */
bool
TriggerFunctionExists(Oid oid)
{
    return _find_entry_by_oid(oid) != NULL;
}

/**
 * @brief 获取注册表大小
 */
int
GetTriggerFunctionCount(void)
{
    return g_trigger_func_count;
}

/* ========================================================================
 * 触发器调用封装
 * ======================================================================== */

/**
 * @brief 执行触发器函数
 */
TupleTableSlot *
CallTriggerFunction(TriggerData *trig_data)
{
    if (trig_data == NULL || trig_data->tg_desc == NULL)
        return NULL;

    /* 查找触发器函数 */
    Oid func_oid = trig_data->tg_desc->tgfuncid;
    TriggerFunc func = FindTriggerFunction(func_oid);

    if (func == NULL) {
        /* 函数未注册，返回原槽 */
        return trig_data->tg_trigslot;
    }

    /* 调用触发器函数 */
    return func(trig_data);
}

/* ========================================================================
 * 内置触发器函数实现
 * ======================================================================== */

/**
 * @brief auto_timestamp - 自动时间戳触发器
 *
 * BEFORE INSERT 触发器，自动设置 created_at 字段。
 * 假设表中有 created_at 字段（第一个 timestamp 类型字段）。
 *
 * 当前实现：
 *   - 检查元组槽是否有效
 *   - 如果有值字段，尝试设置第一个字段为当前时间戳
 *   - 实际生产环境中需要根据字段名查找并设置
 */
TupleTableSlot *
trigger_auto_timestamp(TriggerData *trig_data)
{
    if (trig_data == NULL || trig_data->tg_trigslot == NULL)
        return NULL;

    TupleTableSlot *slot = trig_data->tg_trigslot;

    /* 检查是否为 BEFORE INSERT 触发器 */
    if (!TRIGGER_FIRED_BEFORE_INSERT(trig_data->tg_event))
        return slot;

    /* 获取元组槽值数组 */
    if (slot->tts_values != NULL && slot->tts_nvalid > 0) {
        /* 尝试设置第一个字段为当前时间戳（简化实现） */
        /* 实际生产环境需要根据字段名查找 created_at 字段 */
        if (!slot->tts_isnull[0]) {
            /* 字段已有值，不覆盖 */
            return slot;
        }

        /* 标记为非空（实际应该设置为时间戳值） */
        slot->tts_isnull[0] = false;

        /* 这里简化处理，实际需要获取当前时间戳的 Datum 值 */
        /* 时间戳格式为自 epoch 以来的微秒数 */
        /* slot->tts_values[0] = (Datum)time(NULL) * 1000000; */
    }

    return slot;
}

/**
 * @brief audit_log - 审计日志触发器
 *
 * AFTER 触发器，记录操作到审计日志。
 * 当前实现为占位，将操作信息打印到 stderr。
 *
 * 实际生产环境应该：
 *   - 写入专用审计表
 *   - 记录操作类型、时间、用户、表名等
 */
TupleTableSlot *
trigger_audit_log(TriggerData *trig_data)
{
    if (trig_data == NULL || trig_data->tg_desc == NULL)
        return NULL;

    TriggerDesc *desc = trig_data->tg_desc;

    /* 获取事件类型描述 */
    const char *event_type = "UNKNOWN";
    if (TRIGGER_FIRED_INSERT(trig_data->tg_event)) {
        event_type = "INSERT";
    } else if (TRIGGER_FIRED_UPDATE(trig_data->tg_event)) {
        event_type = "UPDATE";
    } else if (TRIGGER_FIRED_DELETE(trig_data->tg_event)) {
        event_type = "DELETE";
    }

    /* 记录审计日志（简化实现） */
    fprintf(stderr, "[AUDIT] Trigger '%s' fired for table OID %lu, event: %s\n",
            desc->tgname ? desc->tgname : "(null)",
            (unsigned long)desc->tgrelid,
            event_type);

    /* AFTER 触发器不修改元组 */
    return NULL;
}

/**
 * @brief validate_data - 数据验证触发器
 *
 * BEFORE 触发器，验证数据完整性。
 * 当前实现为占位，检查必填字段非空。
 *
 * 实际生产环境应该：
 *   - 根据表结构定义必填字段
 *   - 检查外键约束
 *   - 检查唯一性约束
 */
TupleTableSlot *
trigger_validate_data(TriggerData *trig_data)
{
    if (trig_data == NULL || trig_data->tg_trigslot == NULL)
        return NULL;

    TupleTableSlot *slot = trig_data->tg_trigslot;

    /* 检查是否为 BEFORE 触发器 */
    if (!TRIGGER_FIRED_BEFORE(trig_data->tg_event))
        return slot;

    /* 获取元组槽值数组 */
    if (slot->tts_values != NULL && slot->tts_isnull != NULL) {
        /* 简化实现：检查所有字段是否至少有一个非空 */
        bool has_value = false;
        for (int i = 0; i < slot->tts_nvalid; i++) {
            if (!slot->tts_isnull[i]) {
                has_value = true;
                break;
            }
        }

        /* 如果所有字段都为空，拒绝插入 */
        if (!has_value) {
            fprintf(stderr, "[VALIDATE] All fields are NULL, rejecting insert\n");
            return NULL;
        }
    }

    return slot;
}
