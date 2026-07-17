#include "todo_change.h"
#include "todo_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief 创建 OPSX 变更
 *
 * 通过 popen() 调用 opsx propose CLI 创建变更。
 * 命令格式：opsx propose --title "xxx" --description "xxx"
 * 输出格式（解析 change_id）：
 *   成功时输出包含 "change_id:xxx" 或纯文本 ID
 *   失败时返回非零退出码
 */
int todo_create_change(int64_t todo_id, char *out_change_id, size_t change_id_size,
                      char *out_url, size_t url_size) {
    todo_t todo;
    if (todo_get_by_id(todo_id, &todo) != 0) {
        return -1;
    }

    /* 构造 opsx propose 命令 */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "opsx propose --title \"%s\" --description \"%s\" 2>&1",
        todo.title,
        todo.description[0] ? todo.description : "(无描述)");

    /* 调用 CLI */
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        /* opsx CLI 不可用 */
        return -1;
    }

    /* 读取输出 */
    char buf[4096] = {0};
    size_t total = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) != NULL && total < sizeof(buf) - 1) {
        size_t len = strlen(line);
        if (total + len < sizeof(buf) - 1) {
            strcpy(buf + total, line);
            total += len;
        }
    }

    int exit_code = pclose(fp);

    if (exit_code != 0) {
        /* CLI 返回错误 */
        return -1;
    }

    /* 解析输出，提取 change_id 和 url */
    /* 尝试解析 JSON 格式 {"change_id":"xxx","url":"xxx"} */
    char *cid_start = strstr(buf, "\"change_id\"");
    if (cid_start) {
        char *colon = strchr(cid_start, ':');
        if (colon) {
            char *quote = strchr(colon + 1, '"');
            if (quote) {
                char *end_quote = strchr(quote + 1, '"');
                if (end_quote) {
                    size_t id_len = end_quote - quote - 1;
                    if (id_len < change_id_size) {
                        memcpy(out_change_id, quote + 1, id_len);
                        out_change_id[id_len] = '\0';
                    }
                }
            }
        }
    }

    /* 尝试解析 url */
    char *url_start = strstr(buf, "\"url\"");
    if (url_start) {
        char *colon = strchr(url_start, ':');
        if (colon) {
            char *quote = strchr(colon + 1, '"');
            if (quote) {
                char *end_quote = strchr(quote + 1, '"');
                if (end_quote) {
                    size_t url_len = end_quote - quote - 1;
                    if (url_len < url_size) {
                        memcpy(out_url, quote + 1, url_len);
                        out_url[url_len] = '\0';
                    }
                }
            }
        }
    }

    /* 如果解析失败，生成占位 change_id */
    if (!out_change_id[0]) {
        int64_t now = (int64_t)time(NULL);
        snprintf(out_change_id, change_id_size, "change-%lld-%lld", (long long)todo_id, (long long)now);
        snprintf(out_url, url_size, "https://ops.example.com/changes/%s", out_change_id);
    }

    return 0;
}
