/**
 * @file sql_compat.c
 * @brief SQL 方言兼容层实现
 *
 * 提供多 SQL 方言之间的查询翻译与兼容功能。
 */

#include "db/sql_compat.h"
#include "db/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
/* Windows 没有 strcasestr，使用自定义实现 */
static char* strcasestr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return (char *)haystack;

    for (const char *p = haystack; *p; p++) {
        if (_strnicmp(p, needle, needle_len) == 0) {
            return (char *)p;
        }
    }
    return NULL;
}
#endif

/* ============================================================
 * 方言信息表
 * ============================================================ */

static const char *dialect_names[] = {
    "postgresql",
    "mysql",
    "duckdb",
    "clickhouse",
    "sqlite",
    "internal",
};

/* 方言能力标志 */
static const int dialect_flags[] = {
    /* PostgreSQL: 全部支持 */
    SQL_DIALECT_FLAG_CROSS_JOIN | SQL_DIALECT_FLAG_FULL_OUTER_JOIN |
    SQL_DIALECT_FLAG_LATERAL_JOIN | SQL_DIALECT_FLAG_ARRAY_TYPE |
    SQL_DIALECT_FLAG_JSON_TYPE | SQL_DIALECT_FLAG_WINDOW_FUNCTION |
    SQL_DIALECT_FLAG_CTE | SQL_DIALECT_FLAG_RECURSIVE_CTE |
    SQL_DIALECT_FLAG_UPSERT | SQL_DIALECT_FLAG_BOOL_TYPE |
    SQL_DIALECT_FLAG_ILIKE | SQL_DIALECT_FLAG_REGEXP |
    SQL_DIALECT_FLAG_LIMIT_OFFSET,

    /* MySQL: 大部分支持 */
    SQL_DIALECT_FLAG_CROSS_JOIN | SQL_DIALECT_FLAG_JSON_TYPE |
    SQL_DIALECT_FLAG_WINDOW_FUNCTION | SQL_DIALECT_FLAG_UPSERT |
    SQL_DIALECT_FLAG_REGEXP | SQL_DIALECT_FLAG_LIMIT_OFFSET,

    /* DuckDB: 大部分支持 */
    SQL_DIALECT_FLAG_CROSS_JOIN | SQL_DIALECT_FLAG_FULL_OUTER_JOIN |
    SQL_DIALECT_FLAG_LATERAL_JOIN | SQL_DIALECT_FLAG_ARRAY_TYPE |
    SQL_DIALECT_FLAG_JSON_TYPE | SQL_DIALECT_FLAG_WINDOW_FUNCTION |
    SQL_DIALECT_FLAG_CTE | SQL_DIALECT_FLAG_RECURSIVE_CTE |
    SQL_DIALECT_FLAG_BOOL_TYPE | SQL_DIALECT_FLAG_REGEXP |
    SQL_DIALECT_FLAG_LIMIT_OFFSET,

    /* ClickHouse: 部分支持 */
    SQL_DIALECT_FLAG_CROSS_JOIN | SQL_DIALECT_FLAG_JSON_TYPE |
    SQL_DIALECT_FLAG_CTE | SQL_DIALECT_FLAG_BOOL_TYPE |
    SQL_DIALECT_FLAG_LIMIT_OFFSET,

    /* SQLite: 部分支持 */
    SQL_DIALECT_FLAG_CROSS_JOIN | SQL_DIALECT_FLAG_JSON_TYPE |
    SQL_DIALECT_FLAG_CTE | SQL_DIALECT_FLAG_RECURSIVE_CTE |
    SQL_DIALECT_FLAG_BOOL_TYPE | SQL_DIALECT_FLAG_REGEXP |
    SQL_DIALECT_FLAG_LIMIT_OFFSET,
};

/* ============================================================
 * 类型映射表
 * ============================================================ */

static const sql_type_mapping_t type_mappings[] = {
    /* PostgreSQL -> MySQL */
    {"SERIAL", "INT AUTO_INCREMENT", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"BIGSERIAL", "BIGINT AUTO_INCREMENT", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"BOOLEAN", "TINYINT(1)", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"TEXT", "LONGTEXT", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"BYTEA", "LONGBLOB", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"UUID", "CHAR(36)", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"JSONB", "JSON", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"TIMESTAMPTZ", "TIMESTAMP", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},

    /* MySQL -> PostgreSQL */
    {"INT AUTO_INCREMENT", "SERIAL", 0, 0, SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"BIGINT AUTO_INCREMENT", "BIGSERIAL", 0, 0, SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"TINYINT(1)", "BOOLEAN", 0, 0, SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"LONGTEXT", "TEXT", 0, 0, SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"LONGBLOB", "BYTEA", 0, 0, SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"DATETIME", "TIMESTAMP", 0, 0, SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"DOUBLE", "FLOAT8", 0, 0, SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},

    /* PostgreSQL -> DuckDB */
    {"SERIAL", "INTEGER", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_DUCKDB},
    {"BIGSERIAL", "BIGINT", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_DUCKDB},
    {"JSONB", "JSON", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_DUCKDB},
    {"BYTEA", "BLOB", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_DUCKDB},

    /* PostgreSQL -> ClickHouse */
    {"SERIAL", "UInt32", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_CLICKHOUSE},
    {"BIGSERIAL", "UInt64", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_CLICKHOUSE},
    {"BOOLEAN", "UInt8", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_CLICKHOUSE},
    {"TEXT", "String", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_CLICKHOUSE},
    {"TIMESTAMP", "DateTime", 0, 0, SQL_DIALECT_POSTGRESQL, SQL_DIALECT_CLICKHOUSE},

    /* ClickHouse -> PostgreSQL */
    {"UInt32", "INTEGER", 0, 0, SQL_DIALECT_CLICKHOUSE, SQL_DIALECT_POSTGRESQL},
    {"UInt64", "BIGINT", 0, 0, SQL_DIALECT_CLICKHOUSE, SQL_DIALECT_POSTGRESQL},
    {"UInt8", "BOOLEAN", 0, 0, SQL_DIALECT_CLICKHOUSE, SQL_DIALECT_POSTGRESQL},
    {"String", "TEXT", 0, 0, SQL_DIALECT_CLICKHOUSE, SQL_DIALECT_POSTGRESQL},
    {"DateTime", "TIMESTAMP", 0, 0, SQL_DIALECT_CLICKHOUSE, SQL_DIALECT_POSTGRESQL},
};

static const int num_type_mappings = sizeof(type_mappings) / sizeof(type_mappings[0]);

/* ============================================================
 * 函数映射表
 * ============================================================ */

static const sql_func_mapping_t func_mappings[] = {
    /* PostgreSQL -> MySQL */
    {"now", "NOW()", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"current_timestamp", "NOW()", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"length", "CHAR_LENGTH", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"substr", "SUBSTRING", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {" strpos", "LOCATE", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"array_length", "JSON_LENGTH", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"jsonb_extract_path", "JSON_EXTRACT", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"regexp_matches", "REGEXP", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"ilike", "LIKE", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},
    {"uuid_generate_v4", "UUID()", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_MYSQL},

    /* MySQL -> PostgreSQL */
    {"NOW()", "now", SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"CHAR_LENGTH", "length", SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"LOCATE", "strpos", SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"UUID()", "uuid_generate_v4", SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"IFNULL", "COALESCE", SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},
    {"LIMIT", "LIMIT", SQL_DIALECT_MYSQL, SQL_DIALECT_POSTGRESQL},

    /* PostgreSQL -> DuckDB */
    {"now", "current_timestamp", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_DUCKDB},
    {"jsonb_extract_path", "json_extract", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_DUCKDB},
    {"array_length", "array_length", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_DUCKDB},
    {"regexp_matches", "regexp_extract", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_DUCKDB},
    {"pg_typeof", "typeof", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_DUCKDB},

    /* PostgreSQL -> ClickHouse */
    {"now", "now()", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_CLICKHOUSE},
    {"length", "length", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_CLICKHOUSE},
    {"substr", "substring", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_CLICKHOUSE},
    {"array_length", "length", SQL_DIALECT_POSTGRESQL, SQL_DIALECT_CLICKHOUSE},
};

static const int num_func_mappings = sizeof(func_mappings) / sizeof(func_mappings[0]);

/* ============================================================
 * API 函数实现
 * ============================================================ */

sql_compat_config_t sql_compat_config_default(sql_dialect_t dialect) {
    sql_compat_config_t config;

    config.dialect = dialect;
    config.allow_cross_join = true;
    config.standard_conforming_strings = true;
    config.client_encoding = 6;  /* UTF8 */
    config.case_sensitive = false;
    config.quoted_identifiers = true;

    uint32_t flags = sql_dialect_flags(dialect);
    config.supports_window_func = (flags & SQL_DIALECT_FLAG_WINDOW_FUNCTION) != 0;
    config.supports_recursive_cte = (flags & SQL_DIALECT_FLAG_RECURSIVE_CTE) != 0;
    config.supports_lateral_join = (flags & SQL_DIALECT_FLAG_LATERAL_JOIN) != 0;

    return config;
}

int sql_dialect_flags(sql_dialect_t dialect) {
    if (dialect < 0 || dialect >= SQL_DIALECT_COUNT) return 0;
    return dialect_flags[dialect];
}

bool sql_dialect_supports(sql_dialect_t dialect, uint32_t flag) {
    return (sql_dialect_flags(dialect) & flag) != 0;
}

const char* sql_dialect_name(sql_dialect_t dialect) {
    if (dialect < 0 || dialect >= SQL_DIALECT_COUNT) return "unknown";
    return dialect_names[dialect];
}

sql_dialect_t sql_dialect_parse(const char *name) {
    if (!name) return -1;

    for (int i = 0; i < SQL_DIALECT_COUNT; i++) {
        if (strcasecmp(dialect_names[i], name) == 0) {
            return (sql_dialect_t)i;
        }
    }
    return -1;
}

const char* sql_type_map(const char *source_type, sql_dialect_t from, sql_dialect_t to) {
    if (!source_type) return NULL;

    for (int i = 0; i < num_type_mappings; i++) {
        if (type_mappings[i].source_dialect == from &&
            type_mappings[i].target_dialect == to &&
            strcasecmp(type_mappings[i].source_type, source_type) == 0) {
            return type_mappings[i].target_type;
        }
    }
    return NULL;
}

const char* sql_func_map(const char *source_func, sql_dialect_t from, sql_dialect_t to) {
    if (!source_func) return NULL;

    for (int i = 0; i < num_func_mappings; i++) {
        if (func_mappings[i].source_dialect == from &&
            func_mappings[i].target_dialect == to &&
            strcasecmp(func_mappings[i].source_func, source_func) == 0) {
            return func_mappings[i].target_func;
        }
    }
    return NULL;
}

char* sql_escape_string(const char *input, sql_dialect_t from, sql_dialect_t to) {
    if (!input) return NULL;

    /* 简化实现：只处理基本转义 */
    size_t input_len = strlen(input);
    size_t max_len = input_len * 2 + 1;
    char *output = (char *)malloc(max_len);
    if (!output) return NULL;

    if (from == SQL_DIALECT_MYSQL && to == SQL_DIALECT_POSTGRESQL) {
        /* MySQL 的单引号转义 -> PostgreSQL */
        size_t j = 0;
        for (size_t i = 0; i < input_len; i++) {
            if (input[i] == '\'' && (i + 1 < input_len && input[i + 1] == '\'')) {
                output[j++] = '\'';
                output[j++] = '\'';
                i++;  /* 跳过重复的单引号 */
            } else {
                output[j++] = input[i];
            }
        }
        output[j] = '\0';
    } else if (from == SQL_DIALECT_POSTGRESQL && to == SQL_DIALECT_MYSQL) {
        /* PostgreSQL 的双美元引用 -> MySQL 单引号 */
        /* 检查是否以 $$ 开头 */
        if (strncmp(input, "$$", 2) == 0) {
            /* 跳过首尾 $$ */
            size_t start = 2;
            size_t end = input_len;
            while (end > start && strncmp(input + end - 2, "$$", 2) == 0) {
                end -= 2;
            }
            size_t j = 0;
            for (size_t i = start; i < end; i++) {
                if (input[i] == '\'') {
                    output[j++] = '\\';
                    output[j++] = '\'';
                } else {
                    output[j++] = input[i];
                }
            }
            output[j] = '\0';
        } else {
            strcpy(output, input);
        }
    } else {
        strcpy(output, input);
    }

    return output;
}

char* sql_quote_identifier(const char *identifier, sql_dialect_t dialect) {
    if (!identifier) return NULL;

    size_t len = strlen(identifier);
    size_t max_len = len * 2 + 3;  /* 引号 + null */
    char *output = (char *)malloc(max_len);
    if (!output) return NULL;

    switch (dialect) {
        case SQL_DIALECT_POSTGRESQL:
            /* PostgreSQL: 双引号引用 */
            snprintf(output, max_len, "\"%s\"", identifier);
            break;

        case SQL_DIALECT_MYSQL:
            /* MySQL: 反引号引用 */
            snprintf(output, max_len, "`%s`", identifier);
            break;

        case SQL_DIALECT_DUCKDB:
            /* DuckDB: 双引号引用 */
            snprintf(output, max_len, "\"%s\"", identifier);
            break;

        case SQL_DIALECT_CLICKHOUSE:
            /* ClickHouse: 反引号引用 */
            snprintf(output, max_len, "`%s`", identifier);
            break;

        case SQL_DIALECT_SQLITE:
            /* SQLite: 双引号引用 */
            snprintf(output, max_len, "\"%s\"", identifier);
            break;

        default:
            snprintf(output, max_len, "%s", identifier);
            break;
    }

    return output;
}

int sql_parse_limit_offset(const char *query, int64_t *limit, int64_t *offset) {
    if (!query || !limit || !offset) return -1;

    *limit = -1;
    *offset = 0;

    /* 查找 LIMIT 子句（不区分大小写） */
    const char *p = query;
    while (*p) {
        /* 跳过字符串 */
        if (*p == '\'' || *p == '"') {
            char quote = *p++;
            while (*p && *p != quote) {
                if (*p == '\\') p++;
                p++;
            }
            if (*p) p++;
            continue;
        }

        /* 检查 LIMIT 关键字 */
        if (strncasecmp(p, "LIMIT", 5) == 0 &&
            (p == query || !isalnum((unsigned char)p[-1])) &&
            (p[5] == '\0' || !isalnum((unsigned char)p[5]))) {
            p += 5;

            /* 跳过空白 */
            while (*p && isspace((unsigned char)*p)) p++;

            /* 解析数字 */
            if (*p && isdigit((unsigned char)*p)) {
                *limit = 0;
                while (*p && isdigit((unsigned char)*p)) {
                    *limit = *limit * 10 + (*p - '0');
                    p++;
                }

                /* 跳过空白 */
                while (*p && isspace((unsigned char)*p)) p++;

                /* 检查 OFFSET */
                if (strncasecmp(p, "OFFSET", 6) == 0 &&
                    (p[6] == '\0' || !isalnum((unsigned char)p[6]))) {
                    p += 6;

                    /* 跳过空白 */
                    while (*p && isspace((unsigned char)*p)) p++;

                    if (*p && isdigit((unsigned char)*p)) {
                        *offset = 0;
                        while (*p && isdigit((unsigned char)*p)) {
                            *offset = *offset * 10 + (*p - '0');
                            p++;
                        }
                    }
                }

                return 0;  /* 成功解析 */
            }
        }

        p++;
    }

    return -1;  /* 未找到 LIMIT */
}

/* ============================================================
 * 简单查询翻译（stub 实现）
 * ============================================================ */

static char* translate_limit_offset(const char *query, sql_dialect_t from, sql_dialect_t to) {
    if (!query) return NULL;

    /* 简化实现：只处理 LIMIT/OFFSET 语法差异 */
    size_t len = strlen(query);
    char *result = (char *)malloc(len + 256);
    if (!result) return NULL;

    strcpy(result, query);

    /* MySQL 的 LIMIT offset, count -> LIMIT count OFFSET offset */
    if (from == SQL_DIALECT_MYSQL && to == SQL_DIALECT_POSTGRESQL) {
        const char *limit_pos = strcasestr(result, "LIMIT");
        if (limit_pos) {
            const char *p = limit_pos + 5;
            while (*p && isspace((unsigned char)*p)) p++;

            /* 检查是否有逗号分隔 */
            const char *comma = strchr(p, ',');
            if (comma) {
                /* MySQL 格式: LIMIT offset, count */
                size_t offset_val = 0;
                size_t count_val = 0;

                while (p < comma && isdigit((unsigned char)*p)) {
                    offset_val = offset_val * 10 + (*p - '0');
                    p++;
                }

                p = comma + 1;
                while (*p && isspace((unsigned char)*p)) p++;
                while (*p && isdigit((unsigned char)*p)) {
                    count_val = count_val * 10 + (*p - '0');
                    p++;
                }

                /* 重写为 PostgreSQL 格式 */
                char limit_clause[128];
                snprintf(limit_clause, sizeof(limit_clause), "LIMIT %zu OFFSET %zu",
                         count_val, offset_val);

                /* 找到 LIMIT 子句的结束位置 */
                const char *end = p;
                while (*end && !isspace((unsigned char)*end) &&
                       *end != ';' && *end != ')') end++;

                /* 替换 LIMIT 子句 */
                size_t prefix_len = limit_pos - result;
                size_t suffix_len = strlen(end);
                size_t new_len = prefix_len + strlen(limit_clause) + suffix_len + 1;

                char *new_result = (char *)malloc(new_len);
                if (new_result) {
                    memcpy(new_result, result, prefix_len);
                    strcpy(new_result + prefix_len, limit_clause);
                    strcat(new_result + prefix_len, end);
                    free(result);
                    result = new_result;
                }
            }
        }
    }

    return result;
}

char* sql_translate_query(const char *query, sql_dialect_t from, sql_dialect_t to) {
    if (!query) return NULL;
    if (from == to) return strdup(query);

    /* 简化实现：只做基本的翻译 */
    char *result = translate_limit_offset(query, from, to);
    if (!result) return NULL;

    /* TODO: 完整的查询翻译逻辑 */
    /* - 函数名翻译 */
    /* - 类型名翻译 */
    /* - 语法差异处理 */

    return result;
}
