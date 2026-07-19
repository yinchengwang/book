/**
 * @file guc.c
 * @brief GUC 配置系统实现
 */

#include "db/guc.h"
#include "db/errors.h"
#include "db/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

/*** ============================================================
 * 常量定义
 * ============================================================ */

/** 最大参数数量 */
#define MAX_GUC_PARAMS 256

/** 参数名最大长度 */
#define GUC_NAME_LEN 64

/** 参数值最大长度 */
#define GUC_VALUE_LEN 256

/** ============================================================
 * 静态变量
 * ============================================================ */

/** 参数表 */
static guc_param_t g_guc_params[MAX_GUC_PARAMS];
static int g_num_params = 0;

/** 统计数据 */
static guc_stats_t g_stats = {0, 0, 0};

/** 数据目录 */
static char g_data_dir[512] = {0};

/** ============================================================
 * 内部函数
 * ============================================================ */

/**
 * @brief 查找参数
 */
static guc_param_t *find_param(const char *name) {
    for (int i = 0; i < g_num_params; i++) {
        if (strcasecmp(g_guc_params[i].name, name) == 0) {
            return &g_guc_params[i];
        }
    }
    return NULL;
}

/**
 * @brief 解析内存单位
 * @param value 输入值
 * @param unit 单位字符串
 * @param result 输出值
 * @return 0 成功，-1 失败
 */
static int parse_memory_unit(const char *value, const char *unit, int *result) {
    long val = atol(value);
    if (val < 0) return -1;

    if (strcmp(unit, "kB") == 0) {
        *result = (int)(val * 1024);
    } else if (strcmp(unit, "MB") == 0) {
        *result = (int)(val * 1024 * 1024);
    } else if (strcmp(unit, "GB") == 0) {
        *result = (int)(val * 1024 * 1024 * 1024);
    } else if (strcmp(unit, "TB") == 0) {
        *result = (int)(val * 1024ULL * 1024 * 1024 * 1024);
    } else {
        *result = (int)val;
    }
    return 0;
}

/**
 * @brief 解析时间单位
 * @param value 输入值
 * @param unit 单位字符串
 * @param result 输出值（秒）
 * @return 0 成功，-1 失败
 */
static int parse_time_unit(const char *value, const char *unit, int *result) {
    double val = atof(value);
    if (val < 0) return -1;

    if (strcmp(unit, "ms") == 0) {
        *result = (int)(val / 1000);
    } else if (strcmp(unit, "s") == 0 || strcmp(unit, "sec") == 0) {
        *result = (int)val;
    } else if (strcmp(unit, "min") == 0) {
        *result = (int)(val * 60);
    } else if (strcmp(unit, "h") == 0 || strcmp(unit, "hour") == 0) {
        *result = (int)(val * 3600);
    } else if (strcmp(unit, "d") == 0 || strcmp(unit, "day") == 0) {
        *result = (int)(val * 86400);
    } else {
        *result = (int)val;
    }
    return 0;
}

/**
 * @brief 解析带单位的值
 * @param value 带单位的值字符串
 * @param result 输出值
 * @return 0 成功，-1 失败
 */
static int parse_unit_value(const char *value, int *result) {
    char buf[64];
    char unit[16];
    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* 提取数字和单位 */
    char *p = buf;
    while (*p && (isdigit((unsigned char)*p) || *p == '.' || *p == '-')) p++;
    size_t num_len = p - buf;

    if (num_len == 0) return -1;

    strncpy(unit, p, sizeof(unit) - 1);
    unit[sizeof(unit) - 1] = '\0';

    /* 去掉前导空格 */
    while (*p && isspace((unsigned char)*p)) p++;
    strcpy(unit, p);

    /* 解析 */
    if (strcmp(unit, "B") == 0 || strcmp(unit, "") == 0) {
        *result = atoi(buf);
        return 0;
    }

    return parse_memory_unit(buf, unit, result);
}

/**
 * @brief 解析布尔值
 */
static int parse_bool(const char *value) {
    if (strcasecmp(value, "on") == 0 || strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 || strcmp(value, "1") == 0) {
        return 1;
    }
    if (strcasecmp(value, "off") == 0 || strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "no") == 0 || strcmp(value, "0") == 0) {
        return 0;
    }
    return -1;
}

/**
 * @brief 跳过多余的空白
 */
static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/**
 * @brief 解析配置行
 * @param line 行内容
 * @param name 输出参数名
 * @param value 输出值
 * @return 0 成功，-1 失败，1 跳过（注释/空行）
 */
static int parse_config_line(const char *line, char *name, char *value) {
    const char *p = skip_ws(line);

    /* 跳过空行和注释 */
    if (*p == '\0' || *p == '#' || *p == '\n' || *p == '\r') {
        return 1;
    }

    /* 解析 name = value */
    const char *eq = strchr(p, '=');
    if (!eq) return -1;

    /* 提取参数名 */
    size_t name_len = eq - p;
    while (name_len > 0 && isspace((unsigned char)p[name_len - 1])) name_len--;
    if (name_len == 0 || name_len >= GUC_NAME_LEN) return -1;

    strncpy(name, p, name_len);
    name[name_len] = '\0';

    /* 提取值 */
    const char *val_start = skip_ws(eq + 1);
    const char *val_end = val_start + strlen(val_start);
    while (val_end > val_start && isspace((unsigned char)*(val_end - 1))) val_end--;
    size_t val_len = val_end - val_start;

    /* 去掉引号 */
    if ((val_start[0] == '\'' && val_end[-1] == '\'') ||
        (val_start[0] == '"' && val_end[-1] == '"')) {
        val_start++;
        val_end--;
    }

    if (val_len >= GUC_VALUE_LEN) val_len = GUC_VALUE_LEN - 1;
    strncpy(value, val_start, val_len);
    value[val_len] = '\0';

    return 0;
}

/*** ============================================================
 * 参数注册
 * ============================================================ */

/**
 * @brief 注册布尔参数
 */
static void register_bool(const char *name, bool default_val, const char *desc) {
    if (g_num_params >= MAX_GUC_PARAMS) return;
    guc_param_t *p = &g_guc_params[g_num_params++];
    p->name = name;
    p->type = GUC_TYPE_BOOL;
    p->value = malloc(sizeof(bool));
    p->reset_val = malloc(sizeof(bool));
    *(bool *)p->value = default_val;
    *(bool *)p->reset_val = default_val;
    p->description = desc;
    p->flags = GUC_FLAG_EXPLAIN;
}

/**
 * @brief 注册整数参数
 */
static void register_int(const char *name, int default_val, int min_val, int max_val, const char *desc) {
    if (g_num_params >= MAX_GUC_PARAMS) return;
    guc_param_t *p = &g_guc_params[g_num_params++];
    p->name = name;
    p->type = GUC_TYPE_INT;
    p->value = malloc(sizeof(int));
    p->reset_val = malloc(sizeof(int));
    *(int *)p->value = default_val;
    *(int *)p->reset_val = default_val;
    p->bounds.int_v.min = min_val;
    p->bounds.int_v.max = max_val;
    p->description = desc;
    p->flags = GUC_FLAG_EXPLAIN;
}

/**
 * @brief 注册字符串参数
 */
static void register_string(const char *name, const char *default_val, const char *desc) {
    if (g_num_params >= MAX_GUC_PARAMS) return;
    guc_param_t *p = &g_guc_params[g_num_params++];
    p->name = name;
    p->type = GUC_TYPE_STRING;
    p->value = strdup(default_val ? default_val : "");
    p->reset_val = strdup(default_val ? default_val : "");
    p->description = desc;
    p->flags = GUC_FLAG_EXPLAIN;
}

/*** ============================================================
 * 公共 API
 * ============================================================ */

int guc_init(const char *data_dir) {
    g_num_params = 0;
    memset(&g_stats, 0, sizeof(g_stats));

    if (data_dir) {
        strncpy(g_data_dir, data_dir, sizeof(g_data_dir) - 1);
        g_data_dir[sizeof(g_data_dir) - 1] = '\0';
    }

    /* 注册核心参数 */

    /* 内存参数 */
    register_int("shared_buffers", 16384, 16, 262144,
                 "设置共享缓冲区大小（8KB 页数）");
    register_int("work_mem", 4096, 64, 1048576,
                 "设置排序/哈希操作内存（KB）");
    register_int("maintenance_work_mem", 65536, 1024, 1048576,
                 "设置维护操作内存（KB）");
    register_int("temp_buffers", 1024, 100, 1048576,
                 "设置临时缓冲区大小（KB）");

    /* WAL 参数 */
    register_int("wal_level", 1, 0, 3,
                 "WAL 级别：0=minimal, 1=replica, 2=logical");
    register_bool("fsync", true, "是否同步刷盘");
    register_int("synchronous_commit", 1, 0, 2,
                 "同步提交模式：0=off, 1=on, 2=local");
    register_int("wal_buffers", -1, -1, 4096,
                 "WAL 缓冲区数量（-1=自动）");

    /* 查询规划参数 */
    register_int("random_page_cost", 4, 0, 10000,
                 "随机页访问成本");
    register_int("seq_page_cost", 1, 0, 10000,
                 "顺序页访问成本");
    register_int("effective_cache_size", 524288, 1, INT_MAX,
                 "有效缓存大小（8KB 页）");

    /* 检查点参数 */
    register_int("checkpoint_timeout", 300, 30, 3600,
                 "检查点间隔（秒）");
    register_int("checkpoint_completion_target", 50, 0, 100,
                 "检查点完成目标（百分比）");
    register_int("max_wal_size", 1024, 8, 16384,
                 "最大 WAL 大小（MB）");

    /* 连接参数 */
    register_int("max_connections", 100, 1, 262143,
                 "最大连接数");
    register_string("listen_addresses", "localhost",
                    "监听地址");
    register_int("port", 5432, 1, 65535,
                 "监听端口");

    /* [A1.3] 校验和参数 */
    register_bool("ignore_checksum_failure", false,
                  "校验和失败时是否忽略并继续");

    return 0;
}

void guc_shutdown(void) {
    for (int i = 0; i < g_num_params; i++) {
        free(g_guc_params[i].value);
        free(g_guc_params[i].reset_val);
    }
    g_num_params = 0;
}

int guc_set(const char *name, const char *value) {
    guc_param_t *p = find_param(name);
    if (!p) return -1;

    switch (p->type) {
        case GUC_TYPE_BOOL: {
            int v = parse_bool(value);
            if (v < 0) return -1;
            *(bool *)p->value = (bool)v;
            break;
        }
        case GUC_TYPE_INT: {
            int v;
            if (parse_unit_value(value, &v) != 0) return -1;
            if (v < p->bounds.int_v.min || v > p->bounds.int_v.max) return -1;
            *(int *)p->value = v;
            break;
        }
        case GUC_TYPE_REAL: {
            double v = atof(value);
            if (v < p->bounds.real_v.min || v > p->bounds.real_v.max) return -1;
            *(double *)p->value = v;
            break;
        }
        case GUC_TYPE_STRING: {
            free(p->value);
            p->value = strdup(value);
            break;
        }
        default:
            return -1;
    }

    g_stats.set_count++;
    return 0;
}

int guc_get(const char *name, char *buf, size_t buflen) {
    guc_param_t *p = find_param(name);
    if (!p) return -1;

    switch (p->type) {
        case GUC_TYPE_BOOL:
            snprintf(buf, buflen, "%s", *(bool *)p->value ? "on" : "off");
            break;
        case GUC_TYPE_INT:
            snprintf(buf, buflen, "%d", *(int *)p->value);
            break;
        case GUC_TYPE_REAL:
            snprintf(buf, buflen, "%g", *(double *)p->value);
            break;
        case GUC_TYPE_STRING:
            snprintf(buf, buflen, "%s", (char *)p->value);
            break;
        default:
            return -1;
    }
    return 0;
}

int *guc_get_int(const char *name) {
    guc_param_t *p = find_param(name);
    if (!p || p->type != GUC_TYPE_INT) return NULL;
    return (int *)p->value;
}

bool *guc_get_bool(const char *name) {
    guc_param_t *p = find_param(name);
    if (!p || p->type != GUC_TYPE_BOOL) return NULL;
    return (bool *)p->value;
}

const char *guc_get_string(const char *name) {
    guc_param_t *p = find_param(name);
    if (!p || p->type != GUC_TYPE_STRING) return NULL;
    return (char *)p->value;
}

void guc_reset(const char *name) {
    size_t val_size;
    if (name) {
        guc_param_t *p = find_param(name);
        if (p) {
            val_size = (p->type == GUC_TYPE_STRING) ? strlen((char *)p->reset_val) + 1
                    : (p->type == GUC_TYPE_REAL)   ? sizeof(double)
                    : (p->type == GUC_TYPE_INT)    ? sizeof(int)
                    : sizeof(bool);
            free(p->value);
            p->value = malloc(val_size);
            if (p->value) {
                memcpy(p->value, p->reset_val, val_size);
            }
            g_stats.reset_count++;
        }
    } else {
        /* 重置所有参数 */
        for (int i = 0; i < g_num_params; i++) {
            val_size = (g_guc_params[i].type == GUC_TYPE_STRING) ? strlen((char *)g_guc_params[i].reset_val) + 1
                    : (g_guc_params[i].type == GUC_TYPE_REAL)   ? sizeof(double)
                    : (g_guc_params[i].type == GUC_TYPE_INT)    ? sizeof(int)
                    : sizeof(bool);
            free(g_guc_params[i].value);
            g_guc_params[i].value = malloc(val_size);
            if (g_guc_params[i].value) {
                memcpy(g_guc_params[i].value, g_guc_params[i].reset_val, val_size);
            }
        }
        g_stats.reset_count++;
    }
}

int guc_register_param(const guc_param_t *param) {
    if (param == NULL || param->name == NULL) {
        return -1;
    }
    if (find_param(param->name) != NULL) {
        /* 参数名重复，不允许覆盖 */
        return -1;
    }
    if (g_num_params >= MAX_GUC_PARAMS) {
        return -1;
    }
    guc_param_t *p = &g_guc_params[g_num_params++];
    p->name = param->name;
    p->type = param->type;
    p->description = param->description;
    p->flags = param->flags;
    p->bounds = param->bounds;

    /* 分配并复制值/重置值 */
    size_t val_size = 0;
    if (param->type == GUC_TYPE_BOOL) {
        val_size = sizeof(bool);
    } else if (param->type == GUC_TYPE_INT) {
        val_size = sizeof(int);
    } else if (param->type == GUC_TYPE_REAL) {
        val_size = sizeof(double);
    } else if (param->type == GUC_TYPE_STRING) {
        const char *src = param->value ? (const char *)param->value : "";
        val_size = strlen(src) + 1;
    } else {
        return -1;
    }

    p->value = malloc(val_size);
    if (p->value == NULL) {
        g_num_params--;
        return -1;
    }
    if (param->type == GUC_TYPE_STRING) {
        memcpy(p->value, param->value ? (const char *)param->value : "", val_size);
    } else {
        memcpy(p->value, param->value, val_size);
    }

    p->reset_val = malloc(val_size);
    if (p->reset_val == NULL) {
        free(p->value);
        g_num_params--;
        return -1;
    }
    memcpy(p->reset_val, p->value, val_size);

    return 0;
}

const guc_param_t *guc_get_param(const char *name) {
    return find_param(name);
}

int guc_load_file(const char *path) {
    char filepath[512];
    if (path) {
        strncpy(filepath, path, sizeof(filepath) - 1);
    } else if (g_data_dir[0]) {
        snprintf(filepath, sizeof(filepath), "%s/postgresql.conf", g_data_dir);
    } else {
        return -1;
    }

    FILE *f = fopen(filepath, "r");
    if (!f) return -1;

    char line[1024];
    int errors = 0;

    while (fgets(line, sizeof(line), f)) {
        char name[GUC_NAME_LEN];
        char value[GUC_VALUE_LEN];

        int r = parse_config_line(line, name, value);
        if (r == 1) continue;  /* 注释/空行 */
        if (r < 0) {
            errors++;
            continue;
        }

        if (guc_set(name, value) != 0) {
            LOG_WARN("GUC配置无效: 参数 '%s' 的值 '%s' 无效", name, value);
            errors++;
        }
    }

    fclose(f);
    g_stats.load_count++;
    return errors > 0 ? -1 : 0;
}

int guc_save_file(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;

    fprintf(f, "# PostgreSQL Configuration File\n\n");

    for (int i = 0; i < g_num_params; i++) {
        guc_param_t *p = &g_guc_params[i];
        fprintf(f, "# %s\n", p->description ? p->description : "");
        switch (p->type) {
            case GUC_TYPE_BOOL:
                fprintf(f, "%s = %s\n\n", p->name,
                        *(bool *)p->value ? "on" : "off");
                break;
            case GUC_TYPE_INT:
                fprintf(f, "%s = %d\n\n", p->name, *(int *)p->value);
                break;
            case GUC_TYPE_REAL:
                fprintf(f, "%s = %g\n\n", p->name, *(double *)p->value);
                break;
            case GUC_TYPE_STRING:
                fprintf(f, "%s = '%s'\n\n", p->name, (char *)p->value);
                break;
            default:
                break;
        }
    }

    fclose(f);
    return 0;
}

int guc_enum_index(const guc_param_t *param, const char *value) {
    if (!param || param->type != GUC_TYPE_ENUM) return -1;
    for (int i = 0; i < param->bounds.enum_v.noptions; i++) {
        if (strcasecmp(param->bounds.enum_v.options[i], value) == 0) {
            return i;
        }
    }
    return -1;
}

const char *guc_enum_option(const guc_param_t *param, int index) {
    if (!param || param->type != GUC_TYPE_ENUM) return NULL;
    if (index < 0 || index >= param->bounds.enum_v.noptions) return NULL;
    return param->bounds.enum_v.options[index];
}

void guc_get_stats(guc_stats_t *stats) {
    if (stats) *stats = g_stats;
}

void guc_reset_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
}

int guc_show_all(char **names, char **values, int n) {
    int count = 0;
    for (int i = 0; i < g_num_params && count < n; i++) {
        guc_param_t *p = &g_guc_params[i];
        if (p->flags & GUC_FLAG_NO_SHOW) continue;

        strncpy(names[count], p->name, 63);
        names[count][63] = '\0';

        switch (p->type) {
            case GUC_TYPE_BOOL:
                snprintf(values[count], 255, "%s",
                        *(bool *)p->value ? "on" : "off");
                break;
            case GUC_TYPE_INT:
                snprintf(values[count], 255, "%d", *(int *)p->value);
                break;
            case GUC_TYPE_REAL:
                snprintf(values[count], 255, "%g", *(double *)p->value);
                break;
            case GUC_TYPE_STRING:
                snprintf(values[count], 255, "%s", (char *)p->value);
                break;
            default:
                values[count][0] = '\0';
        }
        count++;
    }
    return count;
}

int guc_reload(const char *path) {
    return guc_load_file(path);
}
