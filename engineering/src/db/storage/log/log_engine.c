/**
 * @file log_engine.c
 * @brief 可观测日志引擎实现（C3-3）
 */
#include "db/storage/log/log_engine_ext.h"
#include "db/core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir_path(p) _mkdir(p)
#else
#include <unistd.h>
#include <dirent.h>
#define mkdir_path(p) mkdir((p), 0755)
#endif

#define LOG_LINE_MAX 4096

struct log_engine_s {
    char data_dir[512];
};

static void stream_id(const log_labels_t *labels, char out[65]) {
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < labels->n_labels; ++i) {
        for (const char *p = labels->keys[i]; *p; ++p) h = (h ^ (uint8_t)*p) * 1099511628211ULL;
        h = (h ^ '=') * 1099511628211ULL;
        for (const char *p = labels->values[i]; *p; ++p) h = (h ^ (uint8_t)*p) * 1099511628211ULL;
        h = (h ^ ';') * 1099511628211ULL;
    }
    snprintf(out, 65, "%016lx", (unsigned long)h);
}

log_engine_t *log_engine_create(const char *data_dir) {
    if (!data_dir) return NULL;
    mkdir_path(data_dir);
    char stream_dir[600];
    snprintf(stream_dir, sizeof(stream_dir), "%s/streams", data_dir);
    mkdir_path(stream_dir);
    log_engine_t *e = calloc(1, sizeof(log_engine_t));
    if (!e) return NULL;
    strncpy(e->data_dir, data_dir, sizeof(e->data_dir) - 1);
    LOG_INFO("日志引擎创建: %s", data_dir);
    return e;
}

log_engine_t *log_engine_open(const char *data_dir) {
    return log_engine_create(data_dir);
}

void log_engine_close(log_engine_t *engine) {
    free(engine);
}

int log_push(log_engine_t *engine, const log_labels_t *labels,
             const log_line_t *lines, size_t n_lines) {
    if (!engine || !labels || !lines) return -1;
    char sid[65];
    stream_id(labels, sid);
    char path[700];
    snprintf(path, sizeof(path), "%s/streams/%s.log",
             engine->data_dir, sid);
    FILE *fp = fopen(path, "ab");
    if (!fp) return -1;
    for (size_t i = 0; i < n_lines; ++i) {
        fwrite(&lines[i].timestamp, sizeof(int64_t), 1, fp);
        uint32_t n = (uint32_t)lines[i].line_len;
        fwrite(&n, sizeof(n), 1, fp);
        if (n > 0) fwrite(lines[i].line, 1, n, fp);
    }
    fflush(fp);
#ifdef _WIN32
    _commit(_fileno(fp));
#else
    fsync(fileno(fp));
#endif
    fclose(fp);
    return 0;
}

/* 辅助：从 log line 文本中提取标签字段值 */
static int extract_label(const char *line, size_t line_len,
                         const char *label_name, char *out_val, size_t val_cap) {
    if (!line || !label_name || !out_val) return -1;
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=", label_name);
    const char *p = strstr(line, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    /* 支持带引号或不带引号的值 */
    if (*p == '"') {
        p++;
        const char *end = strchr(p, '"');
        if (!end) return -1;
        size_t len = (size_t)(end - p);
        if (len >= val_cap) len = val_cap - 1;
        memcpy(out_val, p, len);
        out_val[len] = '\0';
    } else {
        size_t len = 0;
        while (p[len] && !isspace((unsigned char)p[len]) && p[len] != ',') len++;
        if (len >= val_cap) len = val_cap - 1;
        memcpy(out_val, p, len);
        out_val[len] = '\0';
    }
    return 0;
}

/* 简单文件扫描 + 时间 + 关键字过滤 + selector 过滤 */
static int scan_file(const char *path,
                     char **sel_labels, char **sel_values, int sel_n,
                     const char *filter,
                     int64_t start_ms, int64_t end_ms,
                     log_line_t *out, size_t max_out, size_t *count) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    char buf[LOG_LINE_MAX];
    while (*count < max_out) {
        int64_t ts; uint32_t n;
        if (fread(&ts, sizeof(ts), 1, fp) != 1) break;
        if (fread(&n, sizeof(n), 1, fp) != 1) break;
        if (n == 0 || n > LOG_LINE_MAX) break;
        if (fread(buf, 1, n, fp) != n) break;
        buf[n] = '\0';
        if (ts >= start_ms && ts <= end_ms
            && (!filter || strstr(buf, filter))) {
            /* selector 过滤：检查行是否包含所有选择的标签 */
            int matched = 1;
            for (int i = 0; i < sel_n && matched; i++) {
                char val[256];
                if (extract_label(buf, n, sel_labels[i], val, sizeof(val)) == 0) {
                    if (strcmp(val, sel_values[i]) != 0) matched = 0;
                } else {
                    matched = 0;
                }
            }
            if (!matched) continue;
            out[*count].timestamp = ts;
            out[*count].line = strdup(buf);
            out[*count].line_len = n;
            (*count)++;
        }
    }
    fclose(fp);
    return 0;
}

int log_query(log_engine_t *engine, const char *selector,
              const char *filter,
              int64_t start_ms, int64_t end_ms,
              log_line_t *out, size_t max_out, size_t *out_count) {
    if (!engine || !out || !out_count) return -1;
    *out_count = 0;

    /* 解析 selector */
    char **sel_labels = NULL;
    char **sel_values = NULL;
    int sel_n = 0;
    if (selector && *selector) {
        if (log_parse_logql_selector(selector, &sel_labels, &sel_values, &sel_n) != 0) {
            return -1;
        }
    }

    char stream_dir[600];
    snprintf(stream_dir, sizeof(stream_dir), "%s/streams", engine->data_dir);
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    char pattern[700];
    snprintf(pattern, sizeof(pattern), "%s/*.log", stream_dir);
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        log_free_logql_parsed(sel_labels, sel_values, sel_n);
        return 0;
    }
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char path[700]; snprintf(path, sizeof(path), "%s/%s", stream_dir, fd.cFileName);
            scan_file(path, sel_labels, sel_values, sel_n, filter, start_ms, end_ms, out, max_out, out_count);
        }
    } while (FindNextFileA(h, &fd) && *out_count < max_out);
    FindClose(h);
#else
    DIR *d = opendir(stream_dir);
    if (!d) {
        log_free_logql_parsed(sel_labels, sel_values, sel_n);
        return 0;
    }
    struct dirent *de;
    while ((de = readdir(d)) != NULL && *out_count < max_out) {
        if (de->d_name[0] == '.') continue;
        char path[700];
        snprintf(path, sizeof(path), "%s/%s", stream_dir, de->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            scan_file(path, sel_labels, sel_values, sel_n, filter, start_ms, end_ms, out, max_out, out_count);
        }
    }
    closedir(d);
#endif
    log_free_logql_parsed(sel_labels, sel_values, sel_n);
    return 0;
}

int log_rate(log_engine_t *engine, const char *selector,
             int64_t start_ms, int64_t end_ms, int64_t step_ms,
             double *out_values, size_t max_out, size_t *out_count) {
    if (!engine || !out_values || !out_count) return -1;
    *out_count = 0;
    if (step_ms <= 0) return -1;

    /* 分配足够大的缓冲区用于查询 */
    size_t buf_cap = 4096;
    log_line_t *buf = calloc(buf_cap, sizeof(log_line_t));
    if (!buf) return -1;

    int64_t window_start = start_ms;
    while (window_start < end_ms && *out_count < max_out) {
        int64_t window_end = window_start + step_ms;
        if (window_end > end_ms) window_end = end_ms;

        size_t n_matched = 0;
        /* 查询该时间窗口内的日志 */
        int ret = log_query(engine, selector, NULL, window_start, window_end,
                           buf, buf_cap, &n_matched);
        if (ret != 0) {
            free(buf);
            return -1;
        }

        /* 计算速率：count / time_window（秒） */
        double time_window_s = (double)(window_end - window_start) / 1000.0;
        double rate = time_window_s > 0 ? (double)n_matched / time_window_s : 0.0;
        out_values[*out_count] = rate;
        (*out_count)++;

        window_start = window_end;
    }

    /* 释放查询缓冲区 */
    for (size_t i = 0; i < buf_cap; i++) {
        if (buf[i].line) free((char *)buf[i].line);
    }
    free(buf);
    return 0;
}
