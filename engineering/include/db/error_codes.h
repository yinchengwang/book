/*
 * error_codes.h - 统一错误码定义
 *
 * 提供全系统统一的整型错误码，作为各引擎错误码的顶层抽象。
 * 与 SQLSTATE 字符串体系（db/errors.h）和 DB_* 易读格式（db/db_err.h）并存，
 * 用于快速判断和跨引擎传递。
 *
 * 错误码命名规范: ERR_<模块>_<描述>
 * 模块: OK / GENERAL / KV / VEC / GRAPH / TS / DOC / SPATIAL / BLOB 等
 */
#ifndef DB_ERROR_CODES_H
#define DB_ERROR_CODES_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 错误码级别
 * ============================================================ */

typedef enum {
    ERR_LEVEL_SUCCESS = 0,  /* 成功 */
    ERR_LEVEL_WARNING = 1,  /* 警告 */
    ERR_LEVEL_ERROR   = 2,  /* 错误 */
    ERR_LEVEL_FATAL   = 3,  /* 致命错误 */
} err_level_t;

/* ============================================================
 * 统一错误码枚举
 * ============================================================ */

typedef enum {
    /* 通用错误（0-99） */
    ERR_OK = 0,                      /* 成功 */
    ERR_GENERAL = 1,                 /* 一般错误 */

    /* 参数与空指针（10-19） */
    ERR_INVALID_PARAM = 10,          /* 无效参数 */
    ERR_NULL_POINTER  = 11,          /* 空指针 */
    ERR_OUT_OF_RANGE  = 12,          /* 超出范围 */

    /* 内存与资源（20-29） */
    ERR_OUT_OF_MEMORY = 20,          /* 内存不足 */
    ERR_RESOURCE_LIMIT = 21,         /* 资源达到限制 */
    ERR_RESOURCE_BUSY  = 22,         /* 资源忙 */

    /* IO 与文件系统（30-39） */
    ERR_IO_FAILED      = 30,         /* IO 操作失败 */
    ERR_FILE_NOT_FOUND = 31,         /* 文件未找到 */
    ERR_FILE_OPEN_FAILED = 32,       /* 文件打开失败 */
    ERR_READ_FAILED    = 33,         /* 读取失败 */
    ERR_WRITE_FAILED   = 34,         /* 写入失败 */
    ERR_DISK_FULL      = 35,         /* 磁盘空间不足 */

    /* 查找与存在性（40-49） */
    ERR_NOT_FOUND      = 40,         /* 未找到 */
    ERR_EXISTS         = 41,         /* 已存在 */
    ERR_DUPLICATE_KEY  = 42,         /* 重复键 */

    /* 锁与并发（50-59） */
    ERR_LOCK_FAILED    = 50,         /* 锁获取失败 */
    ERR_DEADLOCK       = 51,         /* 死锁 */
    ERR_LOCK_TIMEOUT   = 52,         /* 锁等待超时 */
    ERR_LOCK_NOT_AVAILABLE = 53,      /* 锁不可用 */

    /* 事务（60-69） */
    ERR_TRANSACTION_ROLLBACK = 60,    /* 事务回滚 */
    ERR_SERIALIZATION_FAILURE = 61,   /* 序列化失败 */
    ERR_INVALID_TRANSACTION_STATE = 62, /* 无效事务状态 */
    ERR_NO_ACTIVE_TRANSACTION = 63,   /* 无活动事务 */

    /* WAL 与日志（70-79） */
    ERR_WAL_WRITE_FAILED = 70,       /* WAL 写入失败 */
    ERR_WAL_CORRUPT      = 71,       /* WAL 损坏 */
    ERR_LOG_WRITE_FAILED = 72,       /* 日志写入失败 */

    /* 索引（80-89） */
    ERR_INDEX_CORRUPTED  = 80,        /* 索引损坏 */
    ERR_INDEX_BUILD_FAILED = 81,      /* 索引构建失败 */
    ERR_DUPLICATE_INDEX = 82,        /* 重复索引 */

    /* 存储引擎（90-99） */
    ERR_CORRUPT          = 90,        /* 数据损坏 */
    ERR_FULL             = 91,        /* 存储已满 */
    ERR_CONFLICT         = 92,        /* 冲突（CAS 失败） */
    ERR_TIMEOUT          = 93,        /* 操作超时 */
    ERR_NOT_IMPLEMENTED  = 94,        /* 功能未实现 */

    /* KV 引擎（100-109） */
    ERR_KV = 100,                     /* KV 一般错误 */
    ERR_KV_NOT_FOUND   = 101,         /* KV 键不存在 */
    ERR_KV_CORRUPT     = 102,        /* KV 数据库损坏 */
    ERR_KV_NOMEM       = 103,         /* KV 内存不足 */
    ERR_KV_FULL        = 104,        /* KV 存储已满 */
    ERR_KV_CONFLICT    = 105,        /* KV CAS 冲突 */
    ERR_KV_INVALID     = 106,        /* KV 无效参数 */
    ERR_KV_LOCKED      = 107,        /* KV 键被锁 */

    /* 向量引擎（110-119） */
    ERR_VECTOR = 110,                 /* 向量一般错误 */
    ERR_VECTOR_DIM_MISMATCH = 111,    /* 向量维度不匹配 */
    ERR_VECTOR_NOT_NORMALIZED = 112, /* 向量未归一化 */
    ERR_VECTOR_INVALID_METRIC = 113,  /* 无效距离度量 */
    ERR_VECTOR_INDEX_BUILD_FAILED = 114, /* 向量索引构建失败 */
    ERR_VECTOR_INVALID_VALUE = 115,  /* 无效向量值 */

    /* 图引擎（120-129） */
    ERR_GRAPH = 120,                  /* 图一般错误 */
    ERR_VERTEX_NOT_FOUND = 121,       /* 顶点未找到 */
    ERR_EDGE_NOT_FOUND   = 122,       /* 边未找到 */
    ERR_LABEL_NOT_FOUND  = 123,       /* 标签未找到 */
    ERR_CYCLE_DETECTED   = 124,      /* 检测到环 */
    ERR_INVALID_GRAPH_SCHEMA = 125,   /* 无效图模式 */

    /* 时序引擎（130-139） */
    ERR_TS = 130,                     /* 时序一般错误 */
    ERR_TIMESTAMP_OVERFLOW = 131,     /* 时间戳溢出 */
    ERR_INVALID_TIME_WINDOW = 132,    /* 无效时间窗口 */
    ERR_TIME_SERIES_NOT_FOUND = 133, /* 时间序列未找到 */
    ERR_DOWNSAMPLING_ERROR = 134,     /* 降采样错误 */

    /* 文档引擎（140-149） */
    ERR_DOC = 140,                     /* 文档一般错误 */
    ERR_INVALID_JSON = 141,           /* 无效 JSON */
    ERR_JSON_PATH_ERROR = 142,        /* JSON 路径错误 */
    ERR_DOCUMENT_TOO_LARGE = 143,     /* 文档过大 */
    ERR_SCHEMA_MISMATCH = 144,        /* 模式不匹配 */

    /* 空间引擎（150-159） */
    ERR_SPATIAL = 150,                 /* 空间一般错误 */
    ERR_INVALID_GEOMETRY = 151,       /* 无效几何对象 */
    ERR_GEOMETRY_SRID_MISMATCH = 152, /* 几何 SRID 不匹配 */
    ERR_INVALID_GEOMETRY_TYPE = 153,  /* 无效几何类型 */
    ERR_GEOMETRY_NOT_FOUND = 154,    /* 几何对象未找到 */

    /* BLOB 引擎（160-169） */
    ERR_BLOB = 160,                    /* BLOB 一般错误 */
    ERR_BLOB_NOT_FOUND = 161,         /* BLOB 未找到 */
    ERR_BLOB_CORRUPT = 162,           /* BLOB 损坏 */
    ERR_BLOB_TOO_LARGE = 163,         /* BLOB 过大 */

    /* SQL 与解析（170-179） */
    ERR_SQL = 170,                     /* SQL 一般错误 */
    ERR_SYNTAX_ERROR = 171,           /* 语法错误 */
    ERR_INVALID_NAME = 172,            /* 无效名称 */
    ERR_INVALID_PARAMETER = 173,      /* 无效参数 */
    ERR_DIVISION_BY_ZERO = 174,       /* 除零 */
    ERR_NUMERIC_OVERFLOW = 175,       /* 数值溢出 */

    /* 约束（180-189） */
    ERR_CONSTRAINT = 180,              /* 约束一般错误 */
    ERR_NOT_NULL_VIOLATION = 181,     /* 非空约束违规 */
    ERR_UNIQUE_VIOLATION = 182,       /* 唯一约束违规 */
    ERR_FOREIGN_KEY_VIOLATION = 183,  /* 外键约束违规 */
    ERR_CHECK_VIOLATION = 184,        /* 检查约束违规 */

    /* 权限与认证（190-199） */
    ERR_PERMISSION = 190,              /* 权限一般错误 */
    ERR_INSUFFICIENT_PRIVILEGE = 191, /* 权限不足 */
    ERR_UNAUTHORIZED = 192,           /* 未授权 */

    /* RAG 系统（200-299） */
    ERR_RAG_INTERNAL = 200,            /* RAG 内部错误 */
    ERR_RAG_CONFIG_NOT_FOUND = 201,   /* RAG 配置未找到 */
    ERR_RAG_MODEL_NOT_FOUND = 202,     /* RAG 模型未找到 */
    ERR_RAG_MODEL_LOAD_FAILED = 203,   /* RAG 模型加载失败 */
    ERR_RAG_INDEX_NOT_FOUND = 204,     /* RAG 索引未找到 */
    ERR_RAG_INDEX_BUILD_FAILED = 205, /* RAG 索引构建失败 */
    ERR_RAG_DOC_NOT_FOUND = 206,      /* RAG 文档未找到 */
    ERR_RAG_RETRIEVAL_FAILED = 207,   /* RAG 检索失败 */
    ERR_RAG_LLM_NOT_AVAILABLE = 208,  /* RAG LLM 不可用 */
    ERR_RAG_LLM_GENERATION_FAILED = 209, /* RAG 生成失败 */

    /* 系统错误（900-999） */
    ERR_SYS = 900,                     /* 系统一般错误 */
    ERR_SYS_SYSCALL_FAILED = 901,     /* 系统调用失败 */
    ERR_SYS_PANIC = 902,              /* 严重故障 */
    ERR_SYS_ASSERTION_FAILED = 903,   /* 断言失败 */

    /* 未知错误 */
    ERR_UNKNOWN = 999,                /* 未知错误 */
} ErrorCode;

/* ============================================================
 * 错误码到字符串的转换
 * ============================================================ */

/**
 * @brief 获取错误码对应的可读字符串
 * @param code 错误码
 * @return 错误描述字符串
 */
const char *err_code_to_string(ErrorCode code);

/**
 * @brief 获取错误码对应的简短描述（不含模块前缀）
 * @param code 错误码
 * @return 简短描述字符串
 */
const char *err_code_to_short_string(ErrorCode code);

/**
 * @brief 根据字符串获取错误码
 * @param str 错误码字符串
 * @return 对应的错误码，未知返回 ERR_UNKNOWN
 */
ErrorCode err_code_from_string(const char *str);

/* ============================================================
 * 错误级别辅助函数
 * ============================================================ */

/**
 * @brief 获取错误码对应的错误级别
 * @param code 错误码
 * @return 错误级别
 */
err_level_t err_code_to_level(ErrorCode code);

/**
 * @brief 判断错误码是否为成功
 * @param code 错误码
 * @return true 表示成功
 */
static inline bool err_is_ok(ErrorCode code) {
    return code == ERR_OK;
}

/**
 * @brief 判断错误码是否为错误
 * @param code 错误码
 * @return true 表示错误
 */
static inline bool err_is_error(ErrorCode code) {
    return code != ERR_OK && code < ERR_UNKNOWN;
}

/**
 * @brief 判断是否为警告
 * @param code 错误码
 * @return true 表示警告
 */
static inline bool err_is_warning(ErrorCode code) {
    return code >= 0 && (code % 1000) < 10 && code != ERR_OK;
}

/* ============================================================
 * 与 DBERR_* 的映射（db/errors.h）
 * ============================================================ */

/**
 * @brief 将 DBERR_* 整型错误码转换为统一 ErrorCode
 * @param dberr DBERR_* 错误码
 * @return 对应的 ErrorCode
 */
ErrorCode err_from_dberr(int dberr);

/**
 * @brief 将 ErrorCode 转换为最接近的 DBERR_* 整型错误码
 * @param code ErrorCode
 * @return 对应的 DBERR_* 值
 */
int err_to_dberr(ErrorCode code);

#ifdef __cplusplus
}
#endif

#endif /* DB_ERROR_CODES_H */
