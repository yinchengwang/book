/**
 * @file structured_log.c
 * @brief 结构化日志实现
 */
#include <db/structured_log.h>
#include <db/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* ========================================================================
 * 内部状态
 * ======================================================================== */

static struct {
    pthread_mutex_t mutex;
    StructuredLogConfig config;
    FILE *file;
    bool initialized;
} g_log = {
    .initialized = false
};

static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ========================================================================
 * 颜色定义（ANSI）
 * ======================================================================== */

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

static const char *level_colors[] = {
    [0] = COLOR_CYAN,   /* TRACE */
    [1] = COLOR_CYAN,   /* DEBUG */
    [2] = COLOR_GREEN,  /* INFO */
    [3] = COLOR_YELLOW, /* WARN */
    [4] = COLOR_RED,    /* ERROR */
    [5] = COLOR_RED     /* FATAL */
};

static const char *level_strings[] = {
    [0] = "TRACE",
    [1] = "DEBUG",
    [2] = "INFO",
    [3] = "WARN",
    [4] = "ERROR",
    [5] = "FATAL"
};

/* ========================================================================
 * 时间函数
 * ======================================================================== */

static void get_timestamp(char *buf, size_t buf_size) {
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#else
    struct timeval tv;
    struct tm *tm_info;

    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);

    int ms = tv.tv_usec / 1000;
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", tm_info);
    size_t len = strlen(buf);
    snprintf(buf + len, buf_size - len, ".%03d", ms);
#endif
}

/* ========================================================================
 * 配置和初始化
 * ======================================================================== */

StructuredLogConfig structured_log_default_config(void) {
    StructuredLogConfig config = {
        .min_level = 2,  /* INFO */
        .targets = SLOG_TARGET_STDOUT,
        .file_path = NULL,
        .enable_colors = true,
        .enable_timestamp = true,
        .enable_module = true
    };
    return config;
}

int structured_log_init(const StructuredLogConfig *config) {
    if (!config) return -1;

    pthread_mutex_lock(&g_log_mutex);

    g_log.config = *config;

    /* 打开日志文件 */
    if (config->targets & SLOG_TARGET_FILE && config->file_path) {
        g_log.file = fopen(config->file_path, "a");
        if (!g_log.file) {
            LOG_WARN("Failed to open log file: %s", config->file_path);
        }
    }

    g_log.initialized = true;

    pthread_mutex_unlock(&g_log_mutex);

    LOG_INFO("Structured log initialized, targets: %d, level: %d",
             config->targets, config->min_level);

    return 0;
}

void structured_log_shutdown(void) {
    pthread_mutex_lock(&g_log_mutex);

    if (g_log.file) {
        fclose(g_log.file);
        g_log.file = NULL;
    }

    g_log.initialized = false;

    pthread_mutex_unlock(&g_log_mutex);
}

void structured_log_set_level(int level) {
    pthread_mutex_lock(&g_log_mutex);
    g_log.config.min_level = level;
    pthread_mutex_unlock(&g_log_mutex);
}

/* ========================================================================
 * 日志写入
 * ======================================================================== */

void structured_log_write(int level, const char *module,
                         const char *file, int line,
                         const char *fmt, va_list args) {
    if (!g_log.initialized) return;
    if (level < g_log.config.min_level) return;

    pthread_mutex_lock(&g_log_mutex);

    /* 格式化消息 */
    char message[4096];
    vsnprintf(message, sizeof(message), fmt, args);

    /* 格式化时间戳 */
    char timestamp[64] = {0};
    if (g_log.config.enable_timestamp) {
        get_timestamp(timestamp, sizeof(timestamp));
    }

    /* 构建日志行 */
    char line_buf[8192];
    int offset = 0;

    if (g_log.config.targets & SLOG_TARGET_JSON) {
        /* JSON 格式输出 */
        offset = snprintf(line_buf, sizeof(line_buf),
            "{\"timestamp\":\"%s\",\"level\":\"%s\",\"module\":\"%s\","
            "\"file\":\"%s\",\"line\":%d,\"message\":\"%s\"}",
            timestamp, level_strings[level], module,
            file, line, message);
    } else {
        /* 普通文本格式 */
        if (g_log.config.enable_timestamp) {
            offset += snprintf(line_buf + offset, sizeof(line_buf) - offset,
                             "[%s] ", timestamp);
        }
        offset += snprintf(line_buf + offset, sizeof(line_buf) - offset,
                          "[%s] ", level_strings[level]);
        if (g_log.config.enable_module) {
            offset += snprintf(line_buf + offset, sizeof(line_buf) - offset,
                             "[%s] ", module);
        }
        offset += snprintf(line_buf + offset, sizeof(line_buf) - offset,
                          "%s:%d: %s", file, line, message);
    }

    /* 输出到各个目标 */
    if (g_log.config.targets & SLOG_TARGET_STDOUT) {
        if (g_log.config.enable_colors) {
            printf("%s[%s]%s %s\n", level_colors[level], level_strings[level],
                   COLOR_RESET, line_buf);
        } else {
            printf("%s\n", line_buf);
        }
        fflush(stdout);
    }

    if (g_log.config.targets & SLOG_TARGET_STDERR) {
        fprintf(stderr, "%s\n", line_buf);
        fflush(stderr);
    }

    if (g_log.file) {
        fprintf(g_log.file, "%s\n", line_buf);
        fflush(g_log.file);
    }

    pthread_mutex_unlock(&g_log_mutex);
}

/* ========================================================================
 * 请求日志
 * ======================================================================== */

void structured_log_request(const RequestLogEntry *entry) {
    if (!g_log.initialized || !entry) return;

    pthread_mutex_lock(&g_log_mutex);

    char timestamp[64] = {0};
    get_timestamp(timestamp, sizeof(timestamp));

    /* JSON 格式请求日志 */
    char json_buf[1024];
    snprintf(json_buf, sizeof(json_buf),
        "{\"timestamp\":\"%s\",\"type\":\"request\","
        "\"request_id\":\"%s\",\"method\":\"%s\",\"path\":\"%s\","
        "\"client_ip\":\"%s\",\"status\":%d,\"duration_ms\":%.2f}",
        timestamp,
        entry->request_id ? entry->request_id : "",
        entry->method ? entry->method : "",
        entry->path ? entry->path : "",
        entry->client_ip ? entry->client_ip : "",
        entry->status,
        entry->duration_ms);

    /* 输出到所有目标 */
    if (g_log.config.targets & SLOG_TARGET_STDOUT) {
        printf("%s\n", json_buf);
    }
    if (g_log.file) {
        fprintf(g_log.file, "%s\n", json_buf);
    }

    pthread_mutex_unlock(&g_log_mutex);
}

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

const char *structured_log_level_string(int level) {
    if (level >= 0 && level <= 5) {
        return level_strings[level];
    }
    return "UNKNOWN";
}
