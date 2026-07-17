/**
 * @file log.c
 * @brief 统一日志模块实现
 *
 * 支持分级日志、速率限制、折叠显示。
 */

#include "db/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define LOG_MAX_MESSAGE_LEN    1024    /* 最大消息长度 */
#define LOG_MAX_KEY_LEN        256     /* 抑制键最大长度 */
#define LOG_DEFAULT_RING_SIZE  4096    /* 默认环形缓冲区大小 */
#define LOG_DEFAULT_FILE_SIZE  (10 * 1024 * 1024)  /* 默认文件大小 10MB */
#define LOG_MAX_FILE_COUNT     5       /* 默认保留文件数 */

/* ========================================================================
 * 日志级别配置
 * ======================================================================== */

/* 日志级别名称 */
static const char *level_names[] = {
    "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL", "OFF  "
};

/* ANSI 颜色码 */
static const char *level_colors[] = {
    "\033[90m",   /* TRACE - 灰色 */
    "\033[34m",   /* DEBUG - 蓝色 */
    "\033[0m",    /* INFO - 默认 */
    "\033[33m",   /* WARN - 黄色 */
    "\033[31m",   /* ERROR - 红色 */
    "\033[1;31m", /* FATAL - 加粗红 */
    "\033[0m"     /* OFF - 默认 */
};

#define COLOR_RESET "\033[0m"

/* ========================================================================
 * 抑制条目哈希表节点
 * ======================================================================== */

typedef struct log_hash_node_s {
    char                key[LOG_MAX_KEY_LEN];  /* 唯一标识键 */
    uint64_t            hash;                   /* 哈希值 */
    uint32_t            print_count;            /* 已打印次数 */
    uint32_t            total_count;            /* 总发生次数 */
    time_t              first_time;              /* 首次出现时间 */
    time_t              last_time;               /* 上次打印时间 */
    uint32_t            second_count;            /* 本秒内已打印 */
    time_t              second_reset;             /* 秒计数重置时间 */
    uint32_t            hour_count;              /* 本小时内已打印 */
    time_t              hour_reset;               /* 小时计数重置时间 */
    uint32_t            suppress_since_print;    /* 上次打印后被抑制次数 */
    struct log_hash_node_s *next;               /* 下一个节点 */
    struct log_hash_node_s *prev;               /* 上一个节点 */
} log_hash_node_t;

/* ========================================================================
 * 环形缓冲区
 * ======================================================================== */

typedef struct log_ring_buffer_s {
    log_entry_t *entries;        /* 日志条目数组 */
    uint32_t    size;            /* 缓冲区大小 */
    uint32_t    head;            /* 头指针 */
    uint32_t    tail;            /* 尾指针 */
    uint32_t    count;           /* 当前条目数 */
    pthread_mutex_t lock;         /* 保护锁 */
} log_ring_buffer_t;

/* ========================================================================
 * 全局状态
 * ======================================================================== */

typedef struct log_global_state_s {
    bool                initialized;      /* 初始化标志 */
    log_config_t        config;          /* 当前配置 */
    pthread_mutex_t     write_lock;      /* 写锁 */
    pthread_mutex_t     stats_lock;      /* 统计锁 */
    FILE               *file;           /* 日志文件句柄 */
    pthread_mutex_t     file_lock;       /* 文件锁 */
    log_hash_node_t    *hash_table;      /* 抑制哈希表 */
    log_ring_buffer_t  ring;            /* 环形缓冲区 */
    log_stats_t        stats;           /* 统计信息 */
} log_global_state_t;

static log_global_state_t g_log = {
    .initialized = false,
    .file = NULL,
    .hash_table = NULL,
    .stats = {0, 0, 0, 0}
};

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 获取当前时间（秒）
 */
static time_t get_current_second(void) {
    return time(NULL);
}

/**
 * @brief 获取当前小时
 */
static time_t get_current_hour(void) {
    return get_current_second() / 3600;
}

/**
 * @brief 生成抑制键
 */
static void generate_suppress_key(char *key, size_t size,
                                   const char *file, int line,
                                   const char *func, uint64_t msg_hash) {
    const char *filename = strrchr(file, '/');
    if (filename == NULL) {
        filename = strrchr(file, '\\');
    }
    if (filename == NULL) {
        filename = file;
    } else {
        filename++;
    }

    snprintf(key, size, "%s:%d:%s:%llu", filename, line, func,
             (unsigned long long)msg_hash);
}

/**
 * @brief 简单哈希函数
 */
static uint64_t simple_hash(const char *str) {
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

/**
 * @brief 获取或创建抑制条目
 * @note 线程安全：调用者需持有 write_lock
 */
static log_hash_node_t *get_or_create_entry(const char *key) {
    log_hash_node_t *entry = NULL;

    /* 查找现有条目 */
    for (entry = g_log.hash_table; entry != NULL; entry = entry->next) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
    }

    /* 创建新条目 */
    entry = (log_hash_node_t *)calloc(1, sizeof(log_hash_node_t));
    if (entry == NULL) {
        return NULL;
    }

    strncpy(entry->key, key, LOG_MAX_KEY_LEN - 1);
    entry->first_time = get_current_second();
    entry->second_reset = get_current_second();
    entry->hour_reset = get_current_hour();

    /* 添加到链表头部 */
    entry->next = g_log.hash_table;
    entry->prev = NULL;
    if (g_log.hash_table) {
        g_log.hash_table->prev = entry;
    }
    g_log.hash_table = entry;

    g_log.stats.active_entries++;
    if (g_log.stats.active_entries > g_log.stats.peak_entries) {
        g_log.stats.peak_entries = g_log.stats.active_entries;
    }

    return entry;
}

/**
 * @brief 检查是否应该抑制日志
 */
static bool should_suppress(log_hash_node_t *entry) {
    time_t now_sec = get_current_second();
    time_t now_hour = get_current_hour();

    /* 秒级重置检查 */
    if (entry->second_reset != now_sec) {
        entry->second_count = 0;
        entry->second_reset = now_sec;
    }

    /* 小时级重置检查 */
    if (entry->hour_reset != now_hour) {
        entry->hour_count = 0;
        entry->hour_reset = now_hour;
    }

    /* 秒级检查 */
    if (entry->second_count >= g_log.config.rate_limit.second_burst) {
        return true;
    }

    /* 小时级检查 */
    if (entry->hour_count >= g_log.config.rate_limit.hour_cap) {
        return true;
    }

    return false;
}

/**
 * @brief 更新抑制统计
 */
static void update_suppress_stats(log_hash_node_t *entry) {
    entry->print_count++;
    entry->total_count++;
    entry->second_count++;
    entry->hour_count++;
    entry->last_time = get_current_second();
    entry->suppress_since_print = 0;
}

/**
 * @brief 递增抑制计数
 */
static void increment_suppress_count(log_hash_node_t *entry) {
    entry->total_count++;
    entry->suppress_since_print++;
}

/**
 * @brief 获取文件基础名
 */
static const char *get_basename(const char *path) {
    const char *name = strrchr(path, '/');
    if (name == NULL) {
        name = strrchr(path, '\\');
    }
    if (name == NULL) {
        return path;
    }
    return name + 1;
}

/**
 * @brief 获取格式化的时间戳
 */
static void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_result = localtime(&now);

    /* localtime 失败时使用 UTC 时间 */
    if (tm_result == NULL) {
        snprintf(buf, size, "1970-01-01 00:00:00.000");
        return;
    }

    strftime(buf, size - 16, "%Y-%m-%d %H:%M:%S", tm_result);

    /* 添加毫秒（Windows 不支持 gettimeofday，使用固定值） */
    char ms_buf[16];
    snprintf(ms_buf, sizeof(ms_buf), ".000");
    strcat(buf, ms_buf);
}

/**
 * @brief 写入环形缓冲区
 */
static void write_to_ring(const log_entry_t *entry) {
    if (g_log.ring.entries == NULL) {
        return;
    }

    pthread_mutex_lock(&g_log.ring.lock);

    g_log.ring.entries[g_log.ring.head] = *entry;
    g_log.ring.head = (g_log.ring.head + 1) % g_log.ring.size;

    if (g_log.ring.count < g_log.ring.size) {
        g_log.ring.count++;
    }

    pthread_mutex_unlock(&g_log.ring.lock);
}

/**
 * @brief 输出到控制台
 */
static void write_to_console(log_level_t level,
                              const char *file, int line,
                              const char *message) {
    const char *color = level_colors[level];
    const char *reset = COLOR_RESET;

    if (g_log.config.enable_colors) {
        printf("[%s%s%s] [%s:%d] %s\n",
               color, level_names[level], reset,
               get_basename(file), line, message);
    } else {
        printf("[%s] [%s:%d] %s\n",
               level_names[level],
               get_basename(file), line, message);
    }
}

/**
 * @brief 输出折叠信息
 */
static void write_suppress_info(log_hash_node_t *entry) {
    char first_time_buf[32];
    char last_time_buf[32];
    struct tm *tm_result;

    tm_result = localtime(&entry->first_time);
    strftime(first_time_buf, sizeof(first_time_buf), "%H:%M:%S", tm_result);

    tm_result = localtime(&entry->last_time);
    strftime(last_time_buf, sizeof(last_time_buf), "%H:%M:%S", tm_result);

    printf("\n");
    printf("+-----------------------------------------------------+\n");
    printf("| WARNING: Log suppressed (%u repeats)               |\n",
           entry->suppress_since_print);
    printf("|     First:  %s                                   |\n",
           first_time_buf);
    printf("|     Last:   %s                                   |\n",
           last_time_buf);
    printf("|     Printed: %u / Total: %u                       |\n",
           entry->print_count, entry->total_count);
    printf("|     Limit: 1s/%u, 1h/%u                           |\n",
           g_log.config.rate_limit.second_burst,
           g_log.config.rate_limit.hour_cap);
    printf("+-----------------------------------------------------+\n");
}

/**
 * @brief 输出到文件
 */
static void write_to_file(log_level_t level,
                          const char *file, int line,
                          const char *message) {
    if (g_log.file == NULL) {
        return;
    }

    pthread_mutex_lock(&g_log.file_lock);

    fprintf(g_log.file, "[%s] [%s:%d] %s\n",
            level_names[level],
            get_basename(file), line, message);
    fflush(g_log.file);

    pthread_mutex_unlock(&g_log.file_lock);
}

/* ========================================================================
 * 核心日志输出函数
 * ======================================================================== */

/**
 * @brief 实际输出日志
 */
static bool do_log(log_level_t level, const char *file, int line,
                   const char *func, const char *message) {
    char timestamp[64];
    (void)timestamp;  /* 未使用，避免警告 */
    get_timestamp(timestamp, sizeof(timestamp));
    (void)func;

    /* 构建日志条目 */
    log_entry_t entry;
    entry.timestamp = get_current_second();
    entry.level = level;
    strncpy(entry.file, get_basename(file), sizeof(entry.file) - 1);
    entry.file[sizeof(entry.file) - 1] = '\0';
    entry.line = line;
    strncpy(entry.function, func, sizeof(entry.function) - 1);
    entry.function[sizeof(entry.function) - 1] = '\0';
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';
    entry.suppressed_count = 0;

    /* 输出到各目标 */
    if (g_log.config.target & LOG_TARGET_CONSOLE) {
        write_to_console(level, file, line, message);
    }

    if (g_log.config.target & LOG_TARGET_FILE) {
        write_to_file(level, file, line, message);
    }

    if (g_log.config.target & LOG_TARGET_RING) {
        write_to_ring(&entry);
    }

    return true;
}

/* ========================================================================
 * API 实现
 * ======================================================================== */

/* 初始化锁（静态初始化，无竞态问题） */
static pthread_mutex_t s_init_lock = PTHREAD_MUTEX_INITIALIZER;

int log_init(const log_config_t *config) {
    pthread_mutex_lock(&s_init_lock);

    if (g_log.initialized) {
        pthread_mutex_unlock(&s_init_lock);
        return 0;
    }

    /* 初始化锁 */
    pthread_mutex_init(&g_log.write_lock, NULL);
    pthread_mutex_init(&g_log.stats_lock, NULL);
    pthread_mutex_init(&g_log.file_lock, NULL);

    /* 设置默认配置 */
    g_log.config.level = (config && config->level <= LOG_LEVEL_OFF) ?
                          config->level : LOG_LEVEL_INFO;
    g_log.config.target = (config && config->target) ?
                          config->target : LOG_TARGET_CONSOLE;
    g_log.config.enable_colors = (config) ? config->enable_colors : true;
    g_log.config.enable_rate_limit = (config) ? config->enable_rate_limit : true;
    g_log.config.rate_limit.second_burst = (config) ?
                                           config->rate_limit.second_burst : 2;
    g_log.config.rate_limit.hour_cap = (config) ?
                                       config->rate_limit.hour_cap : 50;
    g_log.config.max_file_size = (config) ?
                                  config->max_file_size : LOG_DEFAULT_FILE_SIZE;
    g_log.config.max_file_count = (config) ?
                                   config->max_file_count : LOG_MAX_FILE_COUNT;

    /* 复制文件路径 */
    if (config && config->file_path) {
        static char file_path_buf[512];
        strncpy(file_path_buf, config->file_path, sizeof(file_path_buf) - 1);
        file_path_buf[sizeof(file_path_buf) - 1] = '\0';
        g_log.config.file_path = file_path_buf;

        /* 打开日志文件 */
        g_log.file = fopen(config->file_path, "a");
    }

    /* 初始化环形缓冲区 */
    uint32_t ring_size = (config && config->ring_buffer_size > 0) ?
                          config->ring_buffer_size : LOG_DEFAULT_RING_SIZE;
    g_log.ring.size = ring_size;
    g_log.ring.entries = (log_entry_t *)calloc(ring_size, sizeof(log_entry_t));
    g_log.ring.head = 0;
    g_log.ring.tail = 0;
    g_log.ring.count = 0;
    pthread_mutex_init(&g_log.ring.lock, NULL);

    /* 初始化统计 */
    memset(&g_log.stats, 0, sizeof(g_log.stats));

    g_log.initialized = true;
    pthread_mutex_unlock(&s_init_lock);
    return 0;
}

int log_reconfigure(const log_config_t *config) {
    if (!g_log.initialized) {
        return log_init(config);
    }

    pthread_mutex_lock(&g_log.write_lock);

    /* 更新配置 */
    if (config) {
        if (config->level <= LOG_LEVEL_OFF) {
            g_log.config.level = config->level;
        }
        g_log.config.target = config->target;
        g_log.config.enable_colors = config->enable_colors;
        g_log.config.enable_rate_limit = config->enable_rate_limit;
        g_log.config.rate_limit = config->rate_limit;
    }

    pthread_mutex_unlock(&g_log.write_lock);
    return 0;
}

log_config_t log_get_config(void) {
    return g_log.config;
}

void log_shutdown(void) {
    if (!g_log.initialized) {
        return;
    }

    pthread_mutex_lock(&g_log.write_lock);

    /* 关闭文件 */
    if (g_log.file) {
        fclose(g_log.file);
        g_log.file = NULL;
    }

    /* 释放哈希表 */
    log_hash_node_t *entry = g_log.hash_table;
    while (entry != NULL) {
        log_hash_node_t *tmp = entry->next;
        free(entry);
        entry = tmp;
    }
    g_log.hash_table = NULL;

    /* 释放环形缓冲区 */
    free(g_log.ring.entries);
    g_log.ring.entries = NULL;

    /* 销毁锁 */
    pthread_mutex_destroy(&g_log.write_lock);
    pthread_mutex_destroy(&g_log.stats_lock);
    pthread_mutex_destroy(&g_log.file_lock);
    pthread_mutex_destroy(&g_log.ring.lock);

    g_log.initialized = false;
}

bool log_is_initialized(void) {
    return g_log.initialized;
}

void log_set_level(log_level_t level) {
    if (level < 0 || level > LOG_LEVEL_OFF) {
        return;
    }
    g_log.config.level = level;
}

log_level_t log_get_level(void) {
    return g_log.config.level;
}

void log_set_rate_limit(bool enable) {
    g_log.config.enable_rate_limit = enable;
}

void log_get_stats(log_stats_t *stats) {
    if (stats == NULL) {
        return;
    }

    pthread_mutex_lock(&g_log.stats_lock);
    *stats = g_log.stats;
    pthread_mutex_unlock(&g_log.stats_lock);
}

void log_reset_stats(void) {
    pthread_mutex_lock(&g_log.stats_lock);
    memset(&g_log.stats, 0, sizeof(g_log.stats));
    pthread_mutex_unlock(&g_log.stats_lock);
}

bool log_log(log_level_t level, const char *file, int line,
             const char *func, const char *fmt, ...) {
    if (!g_log.initialized) {
        /* 未初始化时直接打印到控制台 */
        if (level < 0 || level > LOG_LEVEL_OFF) {
            return false;
        }
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
        return true;
    }

    /* 检查日志级别 */
    if (level < g_log.config.level || level >= LOG_LEVEL_OFF) {
        return false;
    }

    pthread_mutex_lock(&g_log.write_lock);

    /* 格式化消息 */
    char message[LOG_MAX_MESSAGE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);

    /* 生成抑制键 */
    uint64_t msg_hash = simple_hash(message);
    char key[LOG_MAX_KEY_LEN];
    generate_suppress_key(key, sizeof(key), file, line, func, msg_hash);

    /* 获取或创建抑制条目 */
    log_hash_node_t *entry = get_or_create_entry(key);
    if (entry == NULL) {
        pthread_mutex_unlock(&g_log.write_lock);
        return false;
    }

    /* 检查是否应该抑制 */
    bool suppressed = false;
    if (g_log.config.enable_rate_limit && should_suppress(entry)) {
        increment_suppress_count(entry);
        g_log.stats.total_suppressed++;
        suppressed = true;
    }

    if (suppressed) {
        pthread_mutex_unlock(&g_log.write_lock);
        return false;
    }

    /* 输出折叠信息（如果之前被抑制过） */
    if (entry->suppress_since_print > 0) {
        write_suppress_info(entry);
    }

    /* 输出日志 */
    do_log(level, file, line, func, message);

    /* 更新统计 */
    update_suppress_stats(entry);

    pthread_mutex_lock(&g_log.stats_lock);
    g_log.stats.total_logged++;
    pthread_mutex_unlock(&g_log.stats_lock);

    pthread_mutex_unlock(&g_log.write_lock);
    return true;
}

void log_log_force(log_level_t level, const char *file, int line,
                   const char *func, const char *fmt, ...) {
    if (!g_log.initialized) {
        if (level < 0 || level > LOG_LEVEL_OFF) {
            return;
        }
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf("\n");
        return;
    }

    if (level < g_log.config.level || level >= LOG_LEVEL_OFF) {
        return;
    }

    pthread_mutex_lock(&g_log.write_lock);

    /* 格式化消息 */
    char message[LOG_MAX_MESSAGE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);

    /* 强制输出 */
    do_log(level, file, line, func, message);

    pthread_mutex_lock(&g_log.stats_lock);
    g_log.stats.total_logged++;
    pthread_mutex_unlock(&g_log.stats_lock);

    pthread_mutex_unlock(&g_log.write_lock);
}

bool log_logv(log_level_t level, const char *file, int line,
              const char *func, const char *fmt, va_list ap) {
    if (!g_log.initialized) {
        if (level < 0 || level > LOG_LEVEL_OFF) {
            return false;
        }
        vprintf(fmt, ap);
        printf("\n");
        return true;
    }

    if (level < g_log.config.level || level >= LOG_LEVEL_OFF) {
        return false;
    }

    pthread_mutex_lock(&g_log.write_lock);

    /* 格式化消息 */
    char message[LOG_MAX_MESSAGE_LEN];
    vsnprintf(message, sizeof(message), fmt, ap);

    /* 生成抑制键 */
    uint64_t msg_hash = simple_hash(message);
    char key[LOG_MAX_KEY_LEN];
    generate_suppress_key(key, sizeof(key), file, line, func, msg_hash);

    /* 获取或创建抑制条目 */
    log_hash_node_t *entry = get_or_create_entry(key);
    if (entry == NULL) {
        pthread_mutex_unlock(&g_log.write_lock);
        return false;
    }

    /* 检查是否应该抑制 */
    if (g_log.config.enable_rate_limit && should_suppress(entry)) {
        increment_suppress_count(entry);
        g_log.stats.total_suppressed++;
        pthread_mutex_unlock(&g_log.write_lock);
        return false;
    }

    /* 输出日志 */
    do_log(level, file, line, func, message);
    update_suppress_stats(entry);

    pthread_mutex_lock(&g_log.stats_lock);
    g_log.stats.total_logged++;
    pthread_mutex_unlock(&g_log.stats_lock);

    pthread_mutex_unlock(&g_log.write_lock);
    return true;
}

void log_logv_force(log_level_t level, const char *file, int line,
                    const char *func, const char *fmt, va_list ap) {
    if (!g_log.initialized) {
        if (level < 0 || level > LOG_LEVEL_OFF) {
            return;
        }
        vprintf(fmt, ap);
        printf("\n");
        return;
    }

    if (level < g_log.config.level || level >= LOG_LEVEL_OFF) {
        return;
    }

    pthread_mutex_lock(&g_log.write_lock);

    /* 格式化消息 */
    char message[LOG_MAX_MESSAGE_LEN];
    vsnprintf(message, sizeof(message), fmt, ap);

    /* 强制输出 */
    do_log(level, file, line, func, message);

    pthread_mutex_lock(&g_log.stats_lock);
    g_log.stats.total_logged++;
    pthread_mutex_unlock(&g_log.stats_lock);

    pthread_mutex_unlock(&g_log.write_lock);
}

uint32_t log_ring_peek(log_entry_t *out, uint32_t max_count) {
    if (out == NULL || max_count == 0 || g_log.ring.entries == NULL) {
        return 0;
    }

    pthread_mutex_lock(&g_log.ring.lock);

    uint32_t count = (max_count < g_log.ring.count) ?
                     max_count : g_log.ring.count;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (g_log.ring.tail + i) % g_log.ring.size;
        out[i] = g_log.ring.entries[idx];
    }

    pthread_mutex_unlock(&g_log.ring.lock);
    return count;
}

void log_ring_clear(void) {
    if (g_log.ring.entries == NULL) {
        return;
    }

    pthread_mutex_lock(&g_log.ring.lock);
    g_log.ring.head = 0;
    g_log.ring.tail = 0;
    g_log.ring.count = 0;
    pthread_mutex_unlock(&g_log.ring.lock);
}

const char *log_level_name(log_level_t level) {
    if (level < 0 || level > LOG_LEVEL_OFF) {
        return "UNKNOWN";
    }
    return level_names[level];
}

const char *log_level_color(log_level_t level) {
    if (level < 0 || level > LOG_LEVEL_OFF) {
        return COLOR_RESET;
    }
    return level_colors[level];
}
