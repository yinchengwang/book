/**
 * @file log_engine_ext.c
 * @brief log_engine 扩展：LogQL 解析 + WAL 接入 + TTL drop（C6.2-C6.5）
 */
#include "db/storage/log/log_engine_ext.h"
#include "db/storage/wal/wal.h"
#include "db/core/log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define unlink_c(f) _unlink(f)
#else
#include <unistd.h>
#include <dirent.h>
#define unlink_c(f) unlink(f)
#endif

int log_parse_logql_selector(const char *selector,
                           char ***out_labels, char ***out_values, int *out_n) {
    if (!selector || !out_labels || !out_values || !out_n) return -1;
    *out_labels = NULL;
    *out_values = NULL;
    *out_n = 0;
    while (*selector && *selector != '{') selector++;
    if (*selector == '{') selector++;

    int cap = 8;
    char **labels = calloc((size_t)cap, sizeof(char *));
    char **values = calloc((size_t)cap, sizeof(char *));
    if (!labels || !values) { free(labels); free(values); return -1; }

    while (*selector && *selector != '}') {
        while (*selector && isspace((unsigned char)*selector)) selector++;
        if (*selector == '}' || !*selector) break;

        char key[64];
        int nk = 0;
        while (*selector && (isalnum((unsigned char)*selector) || *selector == '_') && nk < 63)
            key[nk++] = *selector++;
        key[nk] = '\0';
        if (nk == 0) break;

        while (*selector && (isspace((unsigned char)*selector) || *selector == '=')) selector++;
        if (*selector != '"') break;
        selector++;
        char val[256];
        int nv = 0;
        while (*selector && *selector != '"' && nv < 255) val[nv++] = *selector++;
        val[nv] = '\0';
        if (*selector == '"') selector++;

        if (*out_n >= cap) {
            cap *= 2;
            labels = realloc(labels, (size_t)cap * sizeof(char *));
            values = realloc(values, (size_t)cap * sizeof(char *));
        }
        labels[*out_n] = strdup(key);
        values[*out_n] = strdup(val);
        (*out_n)++;

        while (*selector && (isspace((unsigned char)*selector) || *selector == ','))
            selector++;
    }

    *out_labels = labels;
    *out_values = values;
    return 0;
}

void log_free_logql_parsed(char **labels, char **values, int n) {
    if (labels) { for (int i = 0; i < n; ++i) free(labels[i]); free(labels); }
    if (values) { for (int i = 0; i < n; ++i) free(values[i]); free(values); }
}

int log_push_wal(void *wal, const log_labels_t *labels,
                const log_line_t *lines, size_t n_lines,
                uint64_t stream_id) {
    (void)labels; (void)lines; (void)n_lines; (void)wal;
    if (!wal) return 0;
    /* 占位：复用 C0-2 的 wal_write_log_append */
    return wal_write_log_append((wal_t *)wal, (uint32_t)stream_id, NULL, 0);
}

int logEngine_drop_expired(log_engine_t *engine, int64_t ttl_ms) {
    if (!engine || ttl_ms <= 0) return -1;
    char stream_dir[600];
    snprintf(stream_dir, sizeof(stream_dir), "%s/streams", engine->data_dir);

    int64_t now_ms = 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    int deleted = 0;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    char pattern[700];
    snprintf(pattern, sizeof(pattern), "%s/*.log", stream_dir);
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    FindClose(h);
#else
    DIR *d = opendir(stream_dir);
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char path[700];
        snprintf(path, sizeof(path), "%s/%s", stream_dir, de->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            int64_t mtime_ms = (int64_t)st.st_mtime * 1000;
            if (now_ms - mtime_ms > ttl_ms) {
                if (unlink_c(path) == 0) deleted++;
            }
        }
    }
    closedir(d);
#endif
    LOG_INFO("logEngine_drop_expired: 删除 %d 个过期 stream (ttl=%lldms)",
             deleted, (long long)ttl_ms);
    return deleted;
}
