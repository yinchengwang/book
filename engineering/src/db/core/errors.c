/**
 * @file errors.c
 * @brief 数据库错误码系统实现
 *
 * 参考 PostgreSQL SQLSTATE 规范实现线程安全的错误管理。
 */
#include "db/errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/* ========================================================================
 * SQLSTATE 到消息的映射表
 * ======================================================================== */

typedef struct {
    const char *code;
    const char *message;
} errcode_entry_t;

static const errcode_entry_t errcode_table[] = {
    /* 成功与警告 */
    {"00000", "Successful completion"},
    {"01000", "Warning"},
    {"01003", "Null value eliminated in set function"},
    {"01004", "String data, right truncation"},

    /* 连接异常 */
    {"08000", "Connection exception"},
    {"08003", "Connection does not exist"},
    {"08006", "Connection failure"},

    /* 功能不支持 */
    {"0A000", "Feature not supported"},

    /* 找不到标识符 */
    {"42000", "Syntax error or access rule violation"},
    {"42601", "Syntax error"},
    {"42602", "Invalid name"},
    {"42622", "Name too long"},
    {"42703", "Undefined column"},
    {"42704", "Undefined object"},
    {"42P01", "Undefined table"},
    {"42712", "Duplicate alias"},
    {"42723", "Duplicate function"},
    {"42P06", "Duplicate schema"},
    {"42P07", "Duplicate table"},

    /* 数据异常 */
    {"22000", "Data exception"},
    {"22002", "Null value, no indicator parameter"},
    {"22003", "Numeric value out of range"},
    {"22004", "Null value not allowed"},
    {"22007", "Invalid datetime format"},
    {"22008", "Datetime field overflow"},
    {"22009", "Invalid time zone"},
    {"22012", "Division by zero"},
    {"22018", "Invalid character value for cast"},
    {"2201B", "Invalid regular expression"},
    {"22021", "Character not in repertoire"},
    {"22023", "Invalid parameter value"},

    /* 完整性约束违规 */
    {"23000", "Integrity constraint violation"},
    {"23502", "Not null violation"},
    {"23503", "Foreign key violation"},
    {"23505", "Unique violation"},
    {"23514", "Check violation"},

    /* 事务状态 */
    {"25000", "Invalid transaction state"},
    {"25P01", "No active SQL transaction"},
    {"25P02", "In failed SQL transaction"},
    {"25P03", "Idle in transaction session timeout"},
    {"40000", "Transaction rollback"},
    {"40001", "Serialization failure"},
    {"40P01", "Deadlock detected"},

    /* 授权 */
    {"28000", "Invalid authorization specification"},
    {"42501", "Insufficient privilege"},
    {"42719", "Role not granted"},

    /* 资源不足 */
    {"53000", "Out of memory"},
    {"53100", "Disk full"},
    {"53200", "Out of disk"},
    {"53300", "Too many connections"},
    {"54000", "Program limit exceeded"},
    {"54001", "Statement too complex"},
    {"54004", "Too many columns"},
    {"54011", "Too many open cursors"},
    {"54012", "Too many indexes on table"},
    {"54023", "Too many tables"},
    {"55P03", "Lock not available"},

    /* 系统错误 */
    {"58030", "I/O error"},
    {"XX000", "Internal error"},
    {"XX001", "File system error"},
    {"XX002", "Data corrupted error"},
    {"XX003", "Index corrupted error"},

    /* 图数据库扩展 */
    {"G0001", "Vertex not found"},
    {"G0002", "Edge not found"},
    {"G0003", "Label not found"},
    {"G0004", "Cycle detected"},
    {"G0005", "Invalid graph schema"},
    {"G0006", "Graph constraint violation"},

    /* 向量数据库扩展 */
    {"V0001", "Vector dimension mismatch"},
    {"V0002", "Vector not normalized"},
    {"V0003", "Invalid metric type"},
    {"V0004", "Index build failed"},
    {"V0005", "Invalid vector value"},

    /* 时序数据库扩展 */
    {"T0001", "Timestamp overflow"},
    {"T0002", "Invalid time window"},
    {"T0003", "Time series not found"},
    {"T0004", "Downsampling error"},

    /* 文档数据库扩展 */
    {"D0001", "Invalid JSON"},
    {"D0002", "JSON path error"},
    {"D0003", "Document too large"},
    {"D0004", "Schema mismatch"},

    /* 空间数据库扩展 */
    {"S0001", "Invalid geometry"},
    {"S0002", "Geometry SRID mismatch"},
    {"S0003", "Invalid geometry type"},
    {"S0004", "Geometry not found"},
};

#define ERRCODE_TABLE_SIZE (sizeof(errcode_table) / sizeof(errcode_entry_t))

/* ========================================================================
 * 线程本地存储
 * ======================================================================== */

/* 线程本地错误对象 */
static __thread db_error_t *tl_current_error = NULL;

/* ========================================================================
 * 错误级别名称
 * ======================================================================== */

static const char *error_level_names[] = {
    "SUCCESS",
    "WARNING",
    "ERROR",
    "FATAL",
    "PANIC"
};

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 在错误码表中查找消息
 */
static const char *find_errcode_message(const char *sqlstate) {
    if (sqlstate == NULL) {
        return "Unknown error";
    }

    for (size_t i = 0; i < ERRCODE_TABLE_SIZE; i++) {
        if (strncmp(errcode_table[i].code, sqlstate, 5) == 0) {
            return errcode_table[i].message;
        }
    }
    return "Unknown error";
}

/**
 * @brief 复制字符串（如果不为 NULL）
 */
static char *safe_strdup(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    return strdup(str);
}

/**
 * @brief 分配并格式化字符串
 */
static char *vformat_string(const char *fmt, va_list ap) {
    if (fmt == NULL) {
        return NULL;
    }

    va_list ap_copy;
    va_copy(ap_copy, ap);

    int len = vsnprintf(NULL, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (len < 0) {
        return NULL;
    }

    char *result = (char *)malloc(len + 1);
    if (result == NULL) {
        return NULL;
    }

    vsnprintf(result, len + 1, fmt, ap);
    return result;
}

/* ========================================================================
 * API 实现
 * ======================================================================== */

int db_error_init(void) {
    /* 目前无需初始化线程本地存储 */
    return 0;
}

void db_error_shutdown(void) {
    db_error_clear();
}

db_error_t *db_error_create(const char *sqlstate, db_error_level_t level,
                             const char *fmt, ...) {
    db_error_t *err = (db_error_t *)malloc(sizeof(db_error_t));
    if (err == NULL) {
        return NULL;
    }

    /* 初始化所有字段 */
    memset(err, 0, sizeof(db_error_t));

    if (sqlstate != NULL) {
        strncpy(err->sqlstate, sqlstate, 5);
        err->sqlstate[5] = '\0';
    } else {
        strcpy(err->sqlstate, "XX000");
    }

    err->level = level;

    /* 格式化消息 */
    if (fmt != NULL) {
        va_list ap;
        va_start(ap, fmt);
        err->message = vformat_string(fmt, ap);
        va_end(ap);
    } else {
        err->message = safe_strdup(find_errcode_message(sqlstate));
    }

    return err;
}

db_error_t *db_error_create_detailed(const char *sqlstate, db_error_level_t level,
                                      const char *message, const char *detail,
                                      const char *hint, ...) {
    db_error_t *err = (db_error_t *)malloc(sizeof(db_error_t));
    if (err == NULL) {
        return NULL;
    }

    /* 初始化所有字段 */
    memset(err, 0, sizeof(db_error_t));

    if (sqlstate != NULL) {
        strncpy(err->sqlstate, sqlstate, 5);
        err->sqlstate[5] = '\0';
    } else {
        strcpy(err->sqlstate, "XX000");
    }

    err->level = level;
    err->message = safe_strdup(message);
    err->detail = safe_strdup(detail);

    /* 格式化 hint */
    if (hint != NULL) {
        va_list ap;
        va_start(ap, hint);
        err->hint = vformat_string(hint, ap);
        va_end(ap);
    }

    return err;
}

void db_error_free(db_error_t *err) {
    if (err == NULL) {
        return;
    }

    free(err->message);
    free(err->detail);
    free(err->hint);
    free(err->schema_name);
    free(err->table_name);
    free(err->column_name);
    free(err->constraint_name);
    free(err);
}

db_error_t *db_error_copy(const db_error_t *err) {
    if (err == NULL) {
        return NULL;
    }

    db_error_t *copy = (db_error_t *)malloc(sizeof(db_error_t));
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, err, sizeof(db_error_t));

    /* 深复制字符串字段 */
    copy->message = safe_strdup(err->message);
    copy->detail = safe_strdup(err->detail);
    copy->hint = safe_strdup(err->hint);
    copy->schema_name = safe_strdup(err->schema_name);
    copy->table_name = safe_strdup(err->table_name);
    copy->column_name = safe_strdup(err->column_name);
    copy->constraint_name = safe_strdup(err->constraint_name);

    return copy;
}

db_error_t *db_error_get(void) {
    return tl_current_error;
}

void db_error_set(db_error_t *err) {
    /* 先清除旧的错误 */
    db_error_clear();

    /* 复制新的错误 */
    if (err != NULL) {
        tl_current_error = db_error_copy(err);
    }
}

void db_error_clear(void) {
    if (tl_current_error != NULL) {
        db_error_free(tl_current_error);
        tl_current_error = NULL;
    }
}

bool db_error_has_error(void) {
    return tl_current_error != NULL;
}

db_error_t *db_error_raise(const char *sqlstate, db_error_level_t level,
                            const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char *message = vformat_string(fmt, ap);
    va_end(ap);

    db_error_t *err = db_error_create(sqlstate, level, "%s", message ? message : "");
    free(message);

    if (err != NULL) {
        /* 设置调用位置 */
        err->file = "unknown";
        err->line = 0;
        err->func = "unknown";

        /* 存储到线程本地 */
        db_error_set(err);
        db_error_free(err);  /* 复制后释放原始对象 */
    }

    return tl_current_error;
}

bool db_error_matches(const db_error_t *err, const char *sqlstate) {
    if (err == NULL || sqlstate == NULL) {
        return false;
    }
    return strncmp(err->sqlstate, sqlstate, 5) == 0;
}

const char *db_errcode_to_message(const char *sqlstate) {
    return find_errcode_message(sqlstate);
}

void db_error_format_message(char *buf, size_t size, const db_error_t *err) {
    if (buf == NULL || size == 0 || err == NULL) {
        return;
    }

    int offset = 0;

    /* 错误码和级别 */
    offset += snprintf(buf + offset, size - offset,
                       "错误码: %s\n", err->sqlstate);
    if (offset >= (int)size) return;

    offset += snprintf(buf + offset, size - offset,
                       "级别: %s\n", db_error_level_name(err->level));
    if (offset >= (int)size) return;

    /* 消息 */
    if (err->message != NULL) {
        offset += snprintf(buf + offset, size - offset,
                           "消息: %s\n", err->message);
        if (offset >= (int)size) return;
    }

    /* 详细信息 */
    if (err->detail != NULL) {
        offset += snprintf(buf + offset, size - offset,
                           "详情: %s\n", err->detail);
        if (offset >= (int)size) return;
    }

    /* 提示 */
    if (err->hint != NULL) {
        offset += snprintf(buf + offset, size - offset,
                           "提示: %s\n", err->hint);
        if (offset >= (int)size) return;
    }

    /* 位置信息 */
    if (err->file != NULL && err->line > 0) {
        offset += snprintf(buf + offset, size - offset,
                           "位置: %s:%d", err->file, err->line);
        if (offset >= (int)size) return;

        if (err->func != NULL) {
            snprintf(buf + offset, size - offset, " (%s)", err->func);
        }
    }
}

const char *db_error_level_name(db_error_level_t level) {
    if (level < 0 || level > DB_ERROR_LEVEL_PANIC) {
        return "UNKNOWN";
    }
    return error_level_names[level];
}
