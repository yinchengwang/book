/**
 * static_files.c - 静态文件服务模块
 *
 * 职责：将非 API 请求路由到指定的静态目录，
 * 提供 MIME 类型检测、路径安全检查和 SPA 回退。
 */
#include "static_files.h"
#include "../../common/http_server.h"
#include "../../common/http_router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* 静态文件目录（由 register_static_files 设置） */
static char g_static_dir[1024] = "";

/**
 * @brief 根据文件扩展名返回 MIME 类型
 */
static const char *guess_mime_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcmp(dot, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(dot, ".js") == 0 || strcmp(dot, ".mjs") == 0) return "application/javascript; charset=utf-8";
    if (strcmp(dot, ".json") == 0) return "application/json; charset=utf-8";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".gif") == 0) return "image/gif";
    if (strcmp(dot, ".svg") == 0) return "image/svg+xml";
    if (strcmp(dot, ".ico") == 0) return "image/x-icon";
    if (strcmp(dot, ".woff") == 0) return "font/woff";
    if (strcmp(dot, ".woff2") == 0) return "font/woff2";
    if (strcmp(dot, ".ttf") == 0) return "font/ttf";
    if (strcmp(dot, ".txt") == 0) return "text/plain; charset=utf-8";
    if (strcmp(dot, ".xml") == 0) return "text/xml; charset=utf-8";
    if (strcmp(dot, ".map") == 0) return "application/json";
    return "application/octet-stream";
}

/**
 * @brief 检查路径是否安全（无路径穿越）
 */
static int is_path_safe(const char *path) {
    /* 禁止 .. */
    if (strstr(path, "..")) return 0;
    /* 禁止反斜杠（Windows 路径分隔符，URL 中不应出现） */
    if (strchr(path, '\\')) return 0;
    return 1;
}

/**
 * @brief 提供静态文件服务
 *
 * 路由未命中时由 main.c 的 request_callback 调用。
 * 流程：路径安全检查 → 尝试打开文件 → SPA 回退(index.html) → 返回内容
 *
 * @param req      HTTP 请求
 * @param resp     HTTP 响应（输出）
 * @param static_dir 静态文件根目录
 * @return 0 成功处理，-1 未找到（调用方应返回 404）
 */
int static_files_serve(const HttpRequest *req, HttpResponse *resp, const char *static_dir) {
    if (!static_dir || !static_dir[0]) return -1;

    const char *url_path = req->path;
    if (strcmp(url_path, "/") == 0) url_path = "/index.html";

    /* 安全检查 */
    if (!is_path_safe(url_path)) return -1;

    /* 拼接完整路径 */
    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s%s", static_dir, url_path);

    /* 尝试打开文件 */
    FILE *f = fopen(full_path, "rb");
    if (!f) {
        /* SPA 回退：如果路径无扩展名，尝试返回 index.html */
        if (url_path[0] == '/' && strchr(url_path + 1, '.') == NULL) {
            snprintf(full_path, sizeof(full_path), "%s/index.html", static_dir);
            f = fopen(full_path, "rb");
        }
        if (!f) return -1;
    }

    /* 读取文件内容 */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = (char *)malloc(fsize + 1);
    if (!content) { fclose(f); return -1; }
    fread(content, 1, fsize, f);
    content[fsize] = '\0';
    fclose(f);

    /* 设置响应 */
    resp->status_code = 200;
    resp->status_msg = "OK";
    resp->content_type = guess_mime_type(url_path);
    resp->body = content;
    resp->body_len = (size_t)fsize;
    return 0;
}

/**
 * @brief 注册静态文件服务（当前由 main.c fallback 调用 serve 函数）
 *
 * 保留此函数以维持模块接口一致性。
 * 实际的静态文件服务通过 static_files_serve() 由 request_callback 调用。
 */
void register_static_files(Router *r, const char *static_dir) {
    (void)r;
    if (static_dir) {
        strncpy(g_static_dir, static_dir, sizeof(g_static_dir) - 1);
    }
}