/**
 * @file error_codes.c
 * @brief 统一错误码实现
 */
#include "db/error_codes.h"
#include "db/errors.h"
#include <stdio.h>
#include <string.h>

/* ============================================================
 * 错误码到字符串的映射表
 * ============================================================ */

typedef struct {
    ErrorCode  code;
    const char *str;
    const char *short_str;
} err_code_entry_t;

static const err_code_entry_t err_code_table[] = {
    /* 通用 */
    {ERR_OK,                 "ERR_OK",                 "OK"},
    {ERR_GENERAL,            "ERR_GENERAL",             "GENERAL"},

    /* 参数与空指针 */
    {ERR_INVALID_PARAM,      "ERR_INVALID_PARAM",       "INVALID_PARAM"},
    {ERR_NULL_POINTER,       "ERR_NULL_POINTER",        "NULL_POINTER"},
    {ERR_OUT_OF_RANGE,       "ERR_OUT_OF_RANGE",        "OUT_OF_RANGE"},

    /* 内存与资源 */
    {ERR_OUT_OF_MEMORY,      "ERR_OUT_OF_MEMORY",       "OUT_OF_MEMORY"},
    {ERR_RESOURCE_LIMIT,     "ERR_RESOURCE_LIMIT",      "RESOURCE_LIMIT"},
    {ERR_RESOURCE_BUSY,      "ERR_RESOURCE_BUSY",      "RESOURCE_BUSY"},

    /* IO 与文件系统 */
    {ERR_IO_FAILED,         "ERR_IO_FAILED",           "IO_FAILED"},
    {ERR_FILE_NOT_FOUND,    "ERR_FILE_NOT_FOUND",      "FILE_NOT_FOUND"},
    {ERR_FILE_OPEN_FAILED,  "ERR_FILE_OPEN_FAILED",    "FILE_OPEN_FAILED"},
    {ERR_READ_FAILED,       "ERR_READ_FAILED",         "READ_FAILED"},
    {ERR_WRITE_FAILED,      "ERR_WRITE_FAILED",        "WRITE_FAILED"},
    {ERR_DISK_FULL,         "ERR_DISK_FULL",           "DISK_FULL"},

    /* 查找与存在性 */
    {ERR_NOT_FOUND,          "ERR_NOT_FOUND",           "NOT_FOUND"},
    {ERR_EXISTS,            "ERR_EXISTS",              "EXISTS"},
    {ERR_DUPLICATE_KEY,     "ERR_DUPLICATE_KEY",       "DUPLICATE_KEY"},

    /* 锁与并发 */
    {ERR_LOCK_FAILED,        "ERR_LOCK_FAILED",         "LOCK_FAILED"},
    {ERR_DEADLOCK,          "ERR_DEADLOCK",            "DEADLOCK"},
    {ERR_LOCK_TIMEOUT,       "ERR_LOCK_TIMEOUT",         "LOCK_TIMEOUT"},
    {ERR_LOCK_NOT_AVAILABLE, "ERR_LOCK_NOT_AVAILABLE",   "LOCK_NOT_AVAILABLE"},

    /* 事务 */
    {ERR_TRANSACTION_ROLLBACK, "ERR_TRANSACTION_ROLLBACK", "TRANSACTION_ROLLBACK"},
    {ERR_SERIALIZATION_FAILURE, "ERR_SERIALIZATION_FAILURE", "SERIALIZATION_FAILURE"},
    {ERR_INVALID_TRANSACTION_STATE, "ERR_INVALID_TRANSACTION_STATE", "INVALID_TRANSACTION_STATE"},
    {ERR_NO_ACTIVE_TRANSACTION, "ERR_NO_ACTIVE_TRANSACTION", "NO_ACTIVE_TRANSACTION"},

    /* WAL 与日志 */
    {ERR_WAL_WRITE_FAILED,   "ERR_WAL_WRITE_FAILED",    "WAL_WRITE_FAILED"},
    {ERR_WAL_CORRUPT,       "ERR_WAL_CORRUPT",         "WAL_CORRUPT"},
    {ERR_LOG_WRITE_FAILED,   "ERR_LOG_WRITE_FAILED",    "LOG_WRITE_FAILED"},

    /* 索引 */
    {ERR_INDEX_CORRUPTED,    "ERR_INDEX_CORRUPTED",     "INDEX_CORRUPTED"},
    {ERR_INDEX_BUILD_FAILED, "ERR_INDEX_BUILD_FAILED",  "INDEX_BUILD_FAILED"},
    {ERR_DUPLICATE_INDEX,   "ERR_DUPLICATE_INDEX",     "DUPLICATE_INDEX"},

    /* 存储引擎 */
    {ERR_CORRUPT,            "ERR_CORRUPT",             "CORRUPT"},
    {ERR_FULL,               "ERR_FULL",                "FULL"},
    {ERR_CONFLICT,           "ERR_CONFLICT",            "CONFLICT"},
    {ERR_TIMEOUT,            "ERR_TIMEOUT",             "TIMEOUT"},
    {ERR_NOT_IMPLEMENTED,    "ERR_NOT_IMPLEMENTED",     "NOT_IMPLEMENTED"},

    /* KV 引擎 */
    {ERR_KV,                 "ERR_KV",                  "KV"},
    {ERR_KV_NOT_FOUND,       "ERR_KV_NOT_FOUND",        "KV_NOT_FOUND"},
    {ERR_KV_CORRUPT,         "ERR_KV_CORRUPT",          "KV_CORRUPT"},
    {ERR_KV_NOMEM,           "ERR_KV_NOMEM",            "KV_NOMEM"},
    {ERR_KV_FULL,            "ERR_KV_FULL",              "KV_FULL"},
    {ERR_KV_CONFLICT,        "ERR_KV_CONFLICT",         "KV_CONFLICT"},
    {ERR_KV_INVALID,         "ERR_KV_INVALID",          "KV_INVALID"},
    {ERR_KV_LOCKED,          "ERR_KV_LOCKED",           "KV_LOCKED"},

    /* 向量引擎 */
    {ERR_VECTOR,             "ERR_VECTOR",              "VECTOR"},
    {ERR_VECTOR_DIM_MISMATCH, "ERR_VECTOR_DIM_MISMATCH", "VECTOR_DIM_MISMATCH"},
    {ERR_VECTOR_NOT_NORMALIZED, "ERR_VECTOR_NOT_NORMALIZED", "VECTOR_NOT_NORMALIZED"},
    {ERR_VECTOR_INVALID_METRIC, "ERR_VECTOR_INVALID_METRIC", "VECTOR_INVALID_METRIC"},
    {ERR_VECTOR_INDEX_BUILD_FAILED, "ERR_VECTOR_INDEX_BUILD_FAILED", "VECTOR_INDEX_BUILD_FAILED"},
    {ERR_VECTOR_INVALID_VALUE, "ERR_VECTOR_INVALID_VALUE", "VECTOR_INVALID_VALUE"},

    /* 图引擎 */
    {ERR_GRAPH,              "ERR_GRAPH",               "GRAPH"},
    {ERR_VERTEX_NOT_FOUND,   "ERR_VERTEX_NOT_FOUND",    "VERTEX_NOT_FOUND"},
    {ERR_EDGE_NOT_FOUND,     "ERR_EDGE_NOT_FOUND",      "EDGE_NOT_FOUND"},
    {ERR_LABEL_NOT_FOUND,    "ERR_LABEL_NOT_FOUND",     "LABEL_NOT_FOUND"},
    {ERR_CYCLE_DETECTED,     "ERR_CYCLE_DETECTED",      "CYCLE_DETECTED"},
    {ERR_INVALID_GRAPH_SCHEMA, "ERR_INVALID_GRAPH_SCHEMA", "INVALID_GRAPH_SCHEMA"},

    /* 时序引擎 */
    {ERR_TS,                 "ERR_TS",                  "TS"},
    {ERR_TIMESTAMP_OVERFLOW, "ERR_TIMESTAMP_OVERFLOW",  "TIMESTAMP_OVERFLOW"},
    {ERR_INVALID_TIME_WINDOW, "ERR_INVALID_TIME_WINDOW", "INVALID_TIME_WINDOW"},
    {ERR_TIME_SERIES_NOT_FOUND, "ERR_TIME_SERIES_NOT_FOUND", "TIME_SERIES_NOT_FOUND"},
    {ERR_DOWNSAMPLING_ERROR, "ERR_DOWNSAMPLING_ERROR", "DOWNSAMPLING_ERROR"},

    /* 文档引擎 */
    {ERR_DOC,                "ERR_DOC",                 "DOC"},
    {ERR_INVALID_JSON,       "ERR_INVALID_JSON",        "INVALID_JSON"},
    {ERR_JSON_PATH_ERROR,    "ERR_JSON_PATH_ERROR",     "JSON_PATH_ERROR"},
    {ERR_DOCUMENT_TOO_LARGE, "ERR_DOCUMENT_TOO_LARGE",  "DOCUMENT_TOO_LARGE"},
    {ERR_SCHEMA_MISMATCH,    "ERR_SCHEMA_MISMATCH",     "SCHEMA_MISMATCH"},

    /* 空间引擎 */
    {ERR_SPATIAL,            "ERR_SPATIAL",             "SPATIAL"},
    {ERR_INVALID_GEOMETRY,   "ERR_INVALID_GEOMETRY",    "INVALID_GEOMETRY"},
    {ERR_GEOMETRY_SRID_MISMATCH, "ERR_GEOMETRY_SRID_MISMATCH", "GEOMETRY_SRID_MISMATCH"},
    {ERR_INVALID_GEOMETRY_TYPE, "ERR_INVALID_GEOMETRY_TYPE", "INVALID_GEOMETRY_TYPE"},
    {ERR_GEOMETRY_NOT_FOUND, "ERR_GEOMETRY_NOT_FOUND",  "GEOMETRY_NOT_FOUND"},

    /* BLOB 引擎 */
    {ERR_BLOB,               "ERR_BLOB",                "BLOB"},
    {ERR_BLOB_NOT_FOUND,     "ERR_BLOB_NOT_FOUND",      "BLOB_NOT_FOUND"},
    {ERR_BLOB_CORRUPT,       "ERR_BLOB_CORRUPT",        "BLOB_CORRUPT"},
    {ERR_BLOB_TOO_LARGE,     "ERR_BLOB_TOO_LARGE",      "BLOB_TOO_LARGE"},

    /* SQL 与解析 */
    {ERR_SQL,                "ERR_SQL",                 "SQL"},
    {ERR_SYNTAX_ERROR,       "ERR_SYNTAX_ERROR",        "SYNTAX_ERROR"},
    {ERR_INVALID_NAME,       "ERR_INVALID_NAME",        "INVALID_NAME"},
    {ERR_INVALID_PARAMETER,  "ERR_INVALID_PARAMETER",    "INVALID_PARAMETER"},
    {ERR_DIVISION_BY_ZERO,   "ERR_DIVISION_BY_ZERO",    "DIVISION_BY_ZERO"},
    {ERR_NUMERIC_OVERFLOW,   "ERR_NUMERIC_OVERFLOW",    "NUMERIC_OVERFLOW"},

    /* 约束 */
    {ERR_CONSTRAINT,         "ERR_CONSTRAINT",          "CONSTRAINT"},
    {ERR_NOT_NULL_VIOLATION, "ERR_NOT_NULL_VIOLATION", "NOT_NULL_VIOLATION"},
    {ERR_UNIQUE_VIOLATION,   "ERR_UNIQUE_VIOLATION",    "UNIQUE_VIOLATION"},
    {ERR_FOREIGN_KEY_VIOLATION, "ERR_FOREIGN_KEY_VIOLATION", "FOREIGN_KEY_VIOLATION"},
    {ERR_CHECK_VIOLATION,    "ERR_CHECK_VIOLATION",     "CHECK_VIOLATION"},

    /* 权限与认证 */
    {ERR_PERMISSION,         "ERR_PERMISSION",          "PERMISSION"},
    {ERR_INSUFFICIENT_PRIVILEGE, "ERR_INSUFFICIENT_PRIVILEGE", "INSUFFICIENT_PRIVILEGE"},
    {ERR_UNAUTHORIZED,       "ERR_UNAUTHORIZED",        "UNAUTHORIZED"},

    /* RAG 系统 */
    {ERR_RAG_INTERNAL,              "ERR_RAG_INTERNAL",             "RAG_INTERNAL"},
    {ERR_RAG_CONFIG_NOT_FOUND,      "ERR_RAG_CONFIG_NOT_FOUND",     "RAG_CONFIG_NOT_FOUND"},
    {ERR_RAG_MODEL_NOT_FOUND,       "ERR_RAG_MODEL_NOT_FOUND",      "RAG_MODEL_NOT_FOUND"},
    {ERR_RAG_MODEL_LOAD_FAILED,     "ERR_RAG_MODEL_LOAD_FAILED",    "RAG_MODEL_LOAD_FAILED"},
    {ERR_RAG_INDEX_NOT_FOUND,       "ERR_RAG_INDEX_NOT_FOUND",      "RAG_INDEX_NOT_FOUND"},
    {ERR_RAG_INDEX_BUILD_FAILED,    "ERR_RAG_INDEX_BUILD_FAILED",   "RAG_INDEX_BUILD_FAILED"},
    {ERR_RAG_DOC_NOT_FOUND,         "ERR_RAG_DOC_NOT_FOUND",       "RAG_DOC_NOT_FOUND"},
    {ERR_RAG_RETRIEVAL_FAILED,      "ERR_RAG_RETRIEVAL_FAILED",     "RAG_RETRIEVAL_FAILED"},
    {ERR_RAG_LLM_NOT_AVAILABLE,     "ERR_RAG_LLM_NOT_AVAILABLE",   "RAG_LLM_NOT_AVAILABLE"},
    {ERR_RAG_LLM_GENERATION_FAILED,  "ERR_RAG_LLM_GENERATION_FAILED", "RAG_LLM_GENERATION_FAILED"},

    /* 系统错误 */
    {ERR_SYS,                "ERR_SYS",                 "SYS"},
    {ERR_SYS_SYSCALL_FAILED, "ERR_SYS_SYSCALL_FAILED",  "SYSCALL_FAILED"},
    {ERR_SYS_PANIC,          "ERR_SYS_PANIC",           "PANIC"},
    {ERR_SYS_ASSERTION_FAILED, "ERR_SYS_ASSERTION_FAILED", "ASSERTION_FAILED"},

    /* 未知 */
    {ERR_UNKNOWN,             "ERR_UNKNOWN",             "UNKNOWN"},
};

#define ERR_CODE_TABLE_SIZE (sizeof(err_code_table) / sizeof(err_code_entry_t))

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

static const err_code_entry_t *find_err_entry(ErrorCode code) {
    for (size_t i = 0; i < ERR_CODE_TABLE_SIZE; i++) {
        if (err_code_table[i].code == code) {
            return &err_code_table[i];
        }
    }
    return NULL;
}

static const err_code_entry_t *find_err_entry_by_str(const char *str) {
    if (str == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < ERR_CODE_TABLE_SIZE; i++) {
        if (strcmp(err_code_table[i].str, str) == 0) {
            return &err_code_table[i];
        }
    }
    return NULL;
}

/* ============================================================
 * API 实现
 * ============================================================ */

const char *err_code_to_string(ErrorCode code) {
    const err_code_entry_t *entry = find_err_entry(code);
    return entry ? entry->str : "ERR_UNKNOWN";
}

const char *err_code_to_short_string(ErrorCode code) {
    const err_code_entry_t *entry = find_err_entry(code);
    return entry ? entry->short_str : "UNKNOWN";
}

ErrorCode err_code_from_string(const char *str) {
    const err_code_entry_t *entry = find_err_entry_by_str(str);
    return entry ? entry->code : ERR_UNKNOWN;
}

err_level_t err_code_to_level(ErrorCode code) {
    if (code == ERR_OK) {
        return ERR_LEVEL_SUCCESS;
    }
    /* 系统错误（900-999）为 FATAL */
    if (code >= ERR_SYS && code <= ERR_SYS_ASSERTION_FAILED) {
        return ERR_LEVEL_FATAL;
    }
    /* 警告级别暂未使用具体码值，统一为 ERROR */
    return ERR_LEVEL_ERROR;
}

/* ============================================================
 * DBERR_* 映射
 * ============================================================ */

ErrorCode err_from_dberr(int dberr) {
    switch (dberr) {
        case DBERR_OK:             return ERR_OK;
        case DBERR_INVALID:        return ERR_INVALID_PARAM;
        case DBERR_IO:             return ERR_IO_FAILED;
        case DBERR_NOMEM:          return ERR_OUT_OF_MEMORY;
        case DBERR_FULL:           return ERR_FULL;
        case DBERR_CONFLICT:       return ERR_CONFLICT;
        case DBERR_NOT_IMPLEMENTED: return ERR_NOT_IMPLEMENTED;
        case DBERR_WAL_FAILED:     return ERR_WAL_WRITE_FAILED;
        case DBERR_NOT_FOUND:      return ERR_NOT_FOUND;
        case DBERR_EXISTS:         return ERR_EXISTS;
        case DBERR_CORRUPT:        return ERR_CORRUPT;
        case DBERR_TIMEOUT:        return ERR_TIMEOUT;
        case DBERR_LOCKED:         return ERR_LOCK_NOT_AVAILABLE;
        default: {
            /* 模块错误码 DBERR_MOD_* */
            if (dberr >= DBERR_MOD_KV && dberr < DBERR_MOD_KV + 100) {
                return ERR_KV;
            }
            if (dberr >= DBERR_MOD_VECTOR && dberr < DBERR_MOD_VECTOR + 100) {
                return ERR_VECTOR;
            }
            if (dberr >= DBERR_MOD_GRAPH && dberr < DBERR_MOD_GRAPH + 100) {
                return ERR_GRAPH;
            }
            if (dberr >= DBERR_MOD_TS && dberr < DBERR_MOD_TS + 100) {
                return ERR_TS;
            }
            if (dberr >= DBERR_MOD_DOC && dberr < DBERR_MOD_DOC + 100) {
                return ERR_DOC;
            }
            if (dberr >= DBERR_MOD_SPATIAL && dberr < DBERR_MOD_SPATIAL + 100) {
                return ERR_SPATIAL;
            }
            if (dberr >= DBERR_MOD_BLOB && dberr < DBERR_MOD_BLOB + 100) {
                return ERR_BLOB;
            }
            return ERR_GENERAL;
        }
    }
}

int err_to_dberr(ErrorCode code) {
    switch (code) {
        case ERR_OK:               return DBERR_OK;
        case ERR_INVALID_PARAM:     return DBERR_INVALID;
        case ERR_IO_FAILED:        return DBERR_IO;
        case ERR_OUT_OF_MEMORY:     return DBERR_NOMEM;
        case ERR_FULL:             return DBERR_FULL;
        case ERR_CONFLICT:         return DBERR_CONFLICT;
        case ERR_NOT_IMPLEMENTED:  return DBERR_NOT_IMPLEMENTED;
        case ERR_WAL_WRITE_FAILED: return DBERR_WAL_FAILED;
        case ERR_NOT_FOUND:        return DBERR_NOT_FOUND;
        case ERR_EXISTS:           return DBERR_EXISTS;
        case ERR_CORRUPT:          return DBERR_CORRUPT;
        case ERR_TIMEOUT:          return DBERR_TIMEOUT;
        case ERR_LOCK_NOT_AVAILABLE:  return DBERR_LOCKED;
        case ERR_KV:
        case ERR_KV_NOT_FOUND:
        case ERR_KV_CORRUPT:
        case ERR_KV_NOMEM:
        case ERR_KV_FULL:
        case ERR_KV_CONFLICT:
        case ERR_KV_INVALID:
        case ERR_KV_LOCKED:
            return DBERR_MOD_KV;
        case ERR_VECTOR:
        case ERR_VECTOR_DIM_MISMATCH:
        case ERR_VECTOR_NOT_NORMALIZED:
        case ERR_VECTOR_INVALID_METRIC:
        case ERR_VECTOR_INDEX_BUILD_FAILED:
        case ERR_VECTOR_INVALID_VALUE:
            return DBERR_MOD_VECTOR;
        case ERR_GRAPH:
        case ERR_VERTEX_NOT_FOUND:
        case ERR_EDGE_NOT_FOUND:
        case ERR_LABEL_NOT_FOUND:
        case ERR_CYCLE_DETECTED:
        case ERR_INVALID_GRAPH_SCHEMA:
            return DBERR_MOD_GRAPH;
        case ERR_TS:
        case ERR_TIMESTAMP_OVERFLOW:
        case ERR_INVALID_TIME_WINDOW:
        case ERR_TIME_SERIES_NOT_FOUND:
        case ERR_DOWNSAMPLING_ERROR:
            return DBERR_MOD_TS;
        case ERR_DOC:
        case ERR_INVALID_JSON:
        case ERR_JSON_PATH_ERROR:
        case ERR_DOCUMENT_TOO_LARGE:
        case ERR_SCHEMA_MISMATCH:
            return DBERR_MOD_DOC;
        case ERR_SPATIAL:
        case ERR_INVALID_GEOMETRY:
        case ERR_GEOMETRY_SRID_MISMATCH:
        case ERR_INVALID_GEOMETRY_TYPE:
        case ERR_GEOMETRY_NOT_FOUND:
            return DBERR_MOD_SPATIAL;
        default:
            return DBERR_MOD_KV;  /* 默认归类为 KV */
    }
}
