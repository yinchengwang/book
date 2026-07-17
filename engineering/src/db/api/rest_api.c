/**
 * @file rest_api.c
 * @brief MiniVecDB REST API 实现
 *
 * 基于 Winsock 的简单 HTTP REST API 服务器。
 */
#include <db/api/rest_api.h>
#include <db/api/vector_api.h>
#include <db/vdb_api.h>
#include <db/index/vector_index/BM25/bm25.h>
#include <db/log.h>
#include <cJSON.h>
#include <uthash.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** 路由项 */
typedef struct Route_s {
    HttpMethod method;
    char *path_pattern;
    RequestHandler handler;
    struct Route_s *next;
} Route;

/** BM25 索引哈希表项 */
typedef struct BM25IndexMapEntry {
    char *collection_name;       /* 哈希 key */
    bm25_t *index;               /* 关联的 BM25 索引 */
    UT_hash_handle hh;           /* uthash 句柄 */
} BM25IndexMapEntry;

/** 路径参数解析结果 */
typedef struct PathParams_s {
    char **keys;
    char **values;
    int count;
} PathParams;

/** Per-request 上下文 */
struct RequestContext_s {
    const char *method;
    const char *url;
    const char *body;
    size_t body_length;
    char *request_id;
    char *client_ip;
    PathParams path_params;
    HttpStatus status;
    char *response_body;
    char response_content_type[64];
    void *user_data;  /* 指向 server->vector_api 等上下文对象，handler 通过此访问后端 */
};

/** 服务器内部状态 */
struct RestServer_s {
    RestServerConfig config;
    SOCKET listen_socket;
    volatile bool running;
    int64_t start_time;
    pthread_mutex_t mutex;
    pthread_t accept_thread;

    /* 路由 */
    Route *routes;

    /* 关闭钩子 */
    struct {
        ShutdownHook hook;
        void *arg;
    } shutdown_hooks[16];
    int shutdown_hook_count;

    /* 指标 */
    int64_t vector_count;
    int32_t collection_count;
    struct {
        uint64_t total_requests;
        uint64_t success_requests;
        uint64_t error_requests;
        double total_duration_ms;
    } stats;

    /* Vector API */
    VectorAPI *vector_api;

    /* VDB 统一 API 句柄 */
    vdb_handle_t *vdb;

    /* BM25 索引映射 (collection_name -> bm25_t*) */
    struct BM25IndexMapEntry *bm25_map;
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/* ========================================================================
 * BM25 索引映射辅助函数
 * ========================================================================
 *
 * 注：bm25_map_put_new / bm25_map_get / bm25_map_remove 已被
 * rest_server_bm25_lookup/ensure/drop 取代（供 handlers.c 使用）。
 * 旧函数保留仅作 internal reference，已标记 unused。
 * ======================================================================== */

__attribute__((unused))
static BM25IndexMapEntry *bm25_map_put_new(RestServer *server, const char *name) {
    BM25IndexMapEntry *entry = NULL;
    HASH_FIND_STR(server->bm25_map, name, entry);
    if (entry) return entry;

    entry = (BM25IndexMapEntry *)calloc(1, sizeof(BM25IndexMapEntry));
    entry->collection_name = strdup(name);
    entry->index = bm25_index_create();
    HASH_ADD_KEYPTR(hh, server->bm25_map, entry->collection_name,
                    strlen(entry->collection_name), entry);
    return entry;
}

__attribute__((unused))
static bm25_t *bm25_map_get(RestServer *server, const char *name) {
    BM25IndexMapEntry *entry = NULL;
    HASH_FIND_STR(server->bm25_map, name, entry);
    return entry ? entry->index : NULL;
}

__attribute__((unused))
static void bm25_map_remove(RestServer *server, const char *name) {
    BM25IndexMapEntry *entry = NULL;
    HASH_FIND_STR(server->bm25_map, name, entry);
    if (!entry) return;
    HASH_DEL(server->bm25_map, entry);
    if (entry->index) bm25_index_drop(entry->index);
    free(entry->collection_name);
    free(entry);
}

/**
 * @brief 生成 UUID v4
 */
static void generate_uuid(char *buf) {
    static const char hex[] = "0123456789abcdef";
    static int initialized = 0;
    static unsigned int seed;

    if (!initialized) {
        seed = (unsigned int)time(NULL) ^ (unsigned int)pthread_self();
        initialized = 1;
    }

    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            buf[i] = '-';
        } else if (i == 14) {
            buf[i] = '4';
        } else if (i == 19) {
            buf[i] = hex[(seed >> 8) & 0x0F];
        } else {
            int idx = (seed = seed * 1103515245 + 12345) >> 16;
            buf[i] = hex[idx & 0x0F];
        }
    }
    buf[36] = '\0';
}

/**
 * @brief 获取当前时间（毫秒）
 */
static int64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/**
 * @brief HTTP 方法字符串转枚举
 */
static HttpMethod parse_method(const char *method) {
    if (strcmp(method, "GET") == 0) return HTTP_GET;
    if (strcmp(method, "POST") == 0) return HTTP_POST;
    if (strcmp(method, "PUT") == 0) return HTTP_PUT;
    if (strcmp(method, "DELETE") == 0) return HTTP_DELETE;
    if (strcmp(method, "PATCH") == 0) return HTTP_PATCH;
    if (strcmp(method, "OPTIONS") == 0) return HTTP_OPTIONS;
    return HTTP_GET;
}

/**
 * @brief HTTP 方法枚举转字符串
 */
static const char *method_to_string(HttpMethod method) {
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_PATCH: return "PATCH";
        case HTTP_OPTIONS: return "OPTIONS";
        default: return "GET";
    }
}

/**
 * @brief 解析路径模式
 */
static void parse_path_pattern(const char *pattern, char ***keys, int *key_count) {
    *keys = NULL;
    *key_count = 0;

    const char *p = pattern;
    while (*p) {
        if (*p == ':') {
            p++;
            const char *start = p;
            while (*p && *p != '/') p++;
            int len = p - start;
            if (len > 0) {
                *key_count += 1;
                *keys = realloc(*keys, (*key_count) * sizeof(char *));
                (*keys)[*key_count - 1] = malloc(len + 1);
                strncpy((*keys)[*key_count - 1], start, len);
                (*keys)[*key_count - 1][len] = '\0';
            }
        } else {
            p++;
        }
    }
}

/**
 * @brief 匹配路径并提取参数
 */
static bool match_path(const char *pattern, const char *path, PathParams *params) {
    parse_path_pattern(pattern, &params->keys, &params->count);

    if (params->count == 0) {
        return strcmp(pattern, path) == 0;
    }

    const char *pp = pattern;
    const char *pl = path;
    params->values = calloc(params->count, sizeof(char *));
    int param_idx = 0;

    while (*pp && *pl) {
        if (*pp == ':') {
            pp++;
            while (*pp && *pp != '/') pp++;
            const char *value_start = pl;
            while (*pl && *pl != '/') pl++;

            int value_len = pl - value_start;
            params->values[param_idx] = malloc(value_len + 1);
            strncpy(params->values[param_idx], value_start, value_len);
            params->values[param_idx][value_len] = '\0';
            param_idx++;
        } else if (*pp == *pl) {
            pp++;
            pl++;
        } else {
            for (int i = 0; i < params->count; i++) {
                free(params->keys[i]);
                if (params->values) free(params->values[i]);
            }
            free(params->keys);
            if (params->values) free(params->values);
            params->keys = NULL;
            params->values = NULL;
            params->count = 0;
            return false;
        }
    }

    return *pp == '\0' && *pl == '\0';
}

/**
 * @brief 查找路由处理器
 */
static Route *find_route(RestServer *server, HttpMethod method, const char *path,
                         PathParams *params) {
    Route *r = server->routes;
    while (r) {
        if (r->method == method && match_path(r->path_pattern, path, params)) {
            return r;
        }
        r = r->next;
    }
    return NULL;
}

/**
 * @brief 释放路径参数
 */
static void free_path_params(PathParams *params) {
    for (int i = 0; i < params->count; i++) {
        free(params->keys[i]);
        if (params->values) free(params->values[i]);
    }
    free(params->keys);
    free(params->values);
    params->keys = NULL;
    params->values = NULL;
    params->count = 0;
}

/**
 * @brief URL 解码
 */
static void url_decode(char *dst, const char *src, size_t dst_size) {
    size_t i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = {src[i+1], src[i+2], '\0'};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

/**
 * @brief 提取 URL 路径（去除查询参数）
 */
static void extract_path(char *dst, const char *url, size_t dst_size) {
    const char *path = url;
    const char *query = strchr(url, '?');
    size_t len = query ? (size_t)(query - url) : strlen(url);
    if (len >= dst_size) len = dst_size - 1;
    strncpy(dst, path, len);
    dst[len] = '\0';
    url_decode(dst, dst, dst_size);
}

/**
 * @brief 发送 HTTP 响应
 */
static void send_http_response(SOCKET client, int status_code, const char *status_msg,
                               const char *body, const char *content_type,
                               const char *request_id) {
    char header[1024];
    int header_len;

    /* 构建响应头 */
    header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        status_code, status_msg,
        content_type ? content_type : "application/json",
        body ? strlen(body) : 0);

    if (request_id) {
        header_len += snprintf(header + header_len, sizeof(header) - header_len,
            "X-Request-ID: %s\r\n", request_id);
    }

    strcat(header, "\r\n");

    /* 发送响应 */
    send(client, header, (int)strlen(header), 0);
    if (body && strlen(body) > 0) {
        send(client, body, (int)strlen(body), 0);
    }
}

/**
 * @brief 解析 HTTP 请求
 */
static int parse_http_request(const char *request, char *method, char *path,
                               char *body, size_t *body_len) {
    /* 解析请求行 */
    const char *path_start = strchr(request, ' ');
    if (!path_start) return -1;

    size_t method_len = path_start - request;
    if (method_len >= 32) return -1;
    strncpy(method, request, method_len);
    method[method_len] = '\0';

    path_start++;
    const char *path_end = strchr(path_start, ' ');
    if (!path_end) return -1;

    size_t path_len = path_end - path_start;
    if (path_len >= 1024) return -1;
    strncpy(path, path_start, path_len);
    path[path_len] = '\0';

    /* 查找请求体 */
    const char *body_start = strstr(request, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        size_t remaining = strlen(request) - (body_start - request);
        if (remaining > 0 && *body_len > 0) {
            size_t copy_len = remaining < (*body_len - 1) ? remaining : (*body_len - 1);
            strncpy(body, body_start, copy_len);
            body[copy_len] = '\0';
            *body_len = copy_len;
        } else {
            body[0] = '\0';
            *body_len = 0;
        }
    } else {
        body[0] = '\0';
        *body_len = 0;
    }

    return 0;
}

/**
 * @brief 获取客户端 IP
 */
static void get_client_ip(char *dst, struct sockaddr_in *addr, size_t dst_size) {
    snprintf(dst, dst_size, "%d.%d.%d.%d",
             addr->sin_addr.S_un.S_un_b.s_b1,
             addr->sin_addr.S_un.S_un_b.s_b2,
             addr->sin_addr.S_un.S_un_b.s_b3,
             addr->sin_addr.S_un.S_un_b.s_b4);
}

/* ========================================================================
 * 请求处理线程
 * ======================================================================== */

static void *request_handler_thread(void *arg) {
    RestServer *server = (RestServer *)arg;

    while (server->running) {
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client = accept(server->listen_socket, (struct sockaddr *)&client_addr, &addr_len);

        if (client == INVALID_SOCKET) {
            if (server->running) {
                Sleep(10);
            }
            continue;
        }

        /* 读取请求 */
        char request[65536];
        int bytes_received = recv(client, request, sizeof(request) - 1, 0);

        if (bytes_received <= 0) {
            closesocket(client);
            continue;
        }

        request[bytes_received] = '\0';

        /* 解析请求 */
        char method[32], path[1024], body[65536];
        size_t body_len = sizeof(body);

        if (parse_http_request(request, method, path, body, &body_len) != 0) {
            char req_id[37];
            generate_uuid(req_id);
            send_http_response(client, 400, "Bad Request",
                             "{\"error\":{\"code\":\"BAD_REQUEST\",\"message\":\"Invalid request\"}}",
                             "application/json", req_id);
            closesocket(client);
            continue;
        }

        /* 获取客户端 IP */
        char client_ip[64];
        get_client_ip(client_ip, &client_addr, sizeof(client_ip));

        /* 查找路由 */
        HttpMethod http_method = parse_method(method);
        PathParams params;
        memset(&params, 0, sizeof(params));
        Route *route = find_route(server, http_method, path, &params);

        char req_id[37];
        generate_uuid(req_id);

        if (!route) {
            send_http_response(client, 404, "Not Found",
                             "{\"error\":{\"code\":\"NOT_FOUND\",\"message\":\"Route not found\"}}",
                             "application/json", req_id);
            closesocket(client);
            free_path_params(&params);
            continue;
        }

        /* 创建请求上下文 */
        RequestContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.method = method;
        ctx.url = path;
        ctx.body = body;
        ctx.body_length = body_len;
        ctx.request_id = req_id;
        ctx.client_ip = client_ip;
        ctx.path_params = params;
        ctx.status = HTTP_OK;
        ctx.response_body = NULL;
        ctx.response_content_type[0] = '\0';
        ctx.user_data = server;  /* handler 通过此访问 RestServer → vector_api / bm25_map */

        /* 调用处理器 */
        int64_t start_time = get_current_time_ms();
        int handler_result = route->handler(&ctx);
        double duration = get_current_time_ms() - start_time;

        /* 记录统计 */
        pthread_mutex_lock(&server->mutex);
        server->stats.total_requests++;
        if (handler_result == 0) {
            server->stats.success_requests++;
        } else {
            server->stats.error_requests++;
        }
        server->stats.total_duration_ms += duration;
        pthread_mutex_unlock(&server->mutex);

        /* 发送响应 */
        const char *content_type = ctx.response_content_type[0] ?
            ctx.response_content_type : "application/json";
        const char *response_body = ctx.response_body ? ctx.response_body : "";

        const char *status_msg = "OK";
        if (ctx.status == HTTP_CREATED) status_msg = "Created";
        else if (ctx.status == HTTP_NO_CONTENT) status_msg = "No Content";
        else if (ctx.status == HTTP_BAD_REQUEST) status_msg = "Bad Request";
        else if (ctx.status == HTTP_NOT_FOUND) status_msg = "Not Found";
        else if (ctx.status == HTTP_INTERNAL_ERROR) status_msg = "Internal Server Error";

        send_http_response(client, ctx.status, status_msg, response_body,
                          content_type, req_id);

        /* 清理 */
        free(ctx.response_body);
        free_path_params(&ctx.path_params);
        closesocket(client);
    }

    return NULL;
}

/* ========================================================================
 * 服务器配置
 * ======================================================================== */

RestServerConfig rest_server_default_config(void) {
    RestServerConfig config = {
        .host = "0.0.0.0",
        .port = 8080,
        .data_dir = "./data",
        .max_request_size = 64 * 1024 * 1024,
        .connection_timeout = 30,
        .shutdown_timeout = 30,
        .enable_cors = false
    };
    return config;
}

/* ========================================================================
 * 服务器生命周期
 * ======================================================================== */

/* 前向声明 */
static int init_signal_handlers(RestServer *server);

void *rest_server_get_bm25_map(RestServer *server) {
    if (!server) return NULL;
    return server->bm25_map;
}

void *rest_server_get_vector_api(RestServer *server) {
    if (!server) return NULL;
    return server->vector_api;
}

void *rest_server_get_vdb(RestServer *server) {
    if (!server) return NULL;
    return server->vdb;
}

/* ---------- 暴露给 handlers.c 用的 BM25 映射辅助函数 ---------- */
void *rest_server_bm25_lookup(RestServer *server, const char *name) {
    if (!server || !name) return NULL;
    BM25IndexMapEntry *e = NULL;
    HASH_FIND_STR(server->bm25_map, name, e);
    return e ? e->index : NULL;
}

void *rest_server_bm25_ensure(RestServer *server, const char *name) {
    if (!server || !name) return NULL;
    BM25IndexMapEntry *e = NULL;
    HASH_FIND_STR(server->bm25_map, name, e);
    if (e) return e->index;
    e = (BM25IndexMapEntry *)calloc(1, sizeof(BM25IndexMapEntry));
    e->collection_name = strdup(name);
    e->index = bm25_index_create();
    HASH_ADD_KEYPTR(hh, server->bm25_map, e->collection_name,
                    strlen(e->collection_name), e);
    return e->index;
}

void rest_server_bm25_drop(RestServer *server, const char *name) {
    if (!server || !name) return;
    BM25IndexMapEntry *e = NULL;
    HASH_FIND_STR(server->bm25_map, name, e);
    if (!e) return;
    HASH_DEL(server->bm25_map, e);
    if (e->index) bm25_index_drop(e->index);
    free(e->collection_name);
    free(e);
}

RestServer *rest_server_create(const RestServerConfig *config) {
    RestServer *server = calloc(1, sizeof(RestServer));
    if (!server) return NULL;

    if (config) {
        server->config = *config;
    } else {
        server->config = rest_server_default_config();
    }

    pthread_mutex_init(&server->mutex, NULL);

    /* 初始化 Winsock */
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);

    /* 创建 socket */
    server->listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->listen_socket == INVALID_SOCKET) {
        LOG_ERROR("Failed to create socket");
        pthread_mutex_destroy(&server->mutex);
        free(server);
        return NULL;
    }

    /* 绑定地址 */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)server->config.port);

    if (strcmp(server->config.host, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, server->config.host, &addr.sin_addr);
    }

    if (bind(server->listen_socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        LOG_ERROR("Failed to bind to port %d", server->config.port);
        closesocket(server->listen_socket);
        pthread_mutex_destroy(&server->mutex);
        free(server);
        return NULL;
    }

    /* 监听 */
    if (listen(server->listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        LOG_ERROR("Failed to listen");
        closesocket(server->listen_socket);
        pthread_mutex_destroy(&server->mutex);
        free(server);
        return NULL;
    }

    /* 创建 Vector API */
    server->vector_api = vector_api_create(server->config.data_dir);
    if (!server->vector_api) {
        LOG_ERROR("Failed to create VectorAPI");
        closesocket(server->listen_socket);
        pthread_mutex_destroy(&server->mutex);
        free(server);
        return NULL;
    }

    /* 创建 VDB 统一 API */
    vdb_config_t vdb_cfg = {0};
    strcpy(vdb_cfg.data_dir, server->config.data_dir);
    vdb_cfg.enable_wal = true;
    server->vdb = vdb_open(server->config.data_dir, &vdb_cfg);

    /* 注册路由处理器 */
    rest_api_register_handlers(server);

    LOG_INFO("REST API server created on %s:%d", server->config.host, server->config.port);
    return server;
}

int rest_server_start(RestServer *server) {
    if (!server) return -1;

    server->running = true;
    server->start_time = get_current_time_ms();

    /* 初始化信号处理器 */
    init_signal_handlers(server);

    /* 启动请求处理线程 */
    pthread_create(&server->accept_thread, NULL, request_handler_thread, server);

    LOG_INFO("REST API server started");
    return 0;
}

void rest_server_stop(RestServer *server) {
    if (!server || !server->running) return;

    server->running = false;

    /* 执行关闭钩子 */
    for (int i = 0; i < server->shutdown_hook_count; i++) {
        if (server->shutdown_hooks[i].hook) {
            server->shutdown_hooks[i].hook(server->shutdown_hooks[i].arg);
        }
    }

    if (server->listen_socket != INVALID_SOCKET) {
        closesocket(server->listen_socket);
        server->listen_socket = INVALID_SOCKET;
    }

    pthread_join(server->accept_thread, NULL);
    LOG_INFO("REST API server stopped");
}

void rest_server_wait(RestServer *server) {
    if (!server) return;
    pthread_join(server->accept_thread, NULL);
}

void rest_server_destroy(RestServer *server) {
    if (!server) return;

    rest_server_stop(server);

    /* 释放 VDB 统一 API */
    if (server->vdb) {
        vdb_close(server->vdb);
    }

    if (server->vector_api) {
        vector_api_destroy(server->vector_api);
    }

    /* 释放 BM25 映射 */
    BM25IndexMapEntry *cur, *tmp;
    HASH_ITER(hh, server->bm25_map, cur, tmp) {
        HASH_DEL(server->bm25_map, cur);
        if (cur->index) bm25_index_drop(cur->index);
        free(cur->collection_name);
        free(cur);
    }

    /* 释放路由 */
    Route *r = server->routes;
    while (r) {
        Route *next = r->next;
        free(r->path_pattern);
        free(r);
        r = next;
    }

    WSACleanup();

    pthread_mutex_destroy(&server->mutex);
    free(server);
}

bool rest_server_is_running(RestServer *server) {
    return server && server->running;
}

int64_t rest_server_get_uptime(RestServer *server) {
    if (!server || !server->running) return 0;
    return (get_current_time_ms() - server->start_time) / 1000;
}

/* ========================================================================
 * 优雅关闭
 * ======================================================================== */

/* 全局服务器实例（用于信号处理） */
static RestServer *g_shutdown_server = NULL;

/**
 * @brief Windows 控制台信号处理器
 */
static BOOL WINAPI console_ctrl_handler(DWORD dw_ctrl_type) {
    if (g_shutdown_server && (dw_ctrl_type == CTRL_C_EVENT ||
        dw_ctrl_type == CTRL_BREAK_EVENT ||
        dw_ctrl_type == CTRL_CLOSE_EVENT)) {
        LOG_INFO("Received shutdown signal, initiating graceful shutdown...");
        rest_server_stop(g_shutdown_server);
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief 初始化信号处理器
 */
static int init_signal_handlers(RestServer *server) {
    /* 保存服务器实例用于信号处理 */
    g_shutdown_server = server;

    /* 设置控制台 Ctrl 处理程序 */
    if (!SetConsoleCtrlHandler(console_ctrl_handler, TRUE)) {
        LOG_WARN("Failed to set console control handler");
        return -1;
    }

    LOG_INFO("Signal handlers initialized");
    return 0;
}

/* ========================================================================
 * 关闭钩子
 * ======================================================================== */

int rest_server_register_shutdown_hook(RestServer *server, ShutdownHook hook, void *arg) {
    if (!server || !hook) return -1;
    if (server->shutdown_hook_count >= 16) return -1;

    server->shutdown_hooks[server->shutdown_hook_count].hook = hook;
    server->shutdown_hooks[server->shutdown_hook_count].arg = arg;
    server->shutdown_hook_count++;

    return 0;
}

/* ========================================================================
 * 请求上下文实现
 * ======================================================================== */

const char *request_get_path(RequestContext *ctx) {
    return ctx ? ctx->url : "";
}

HttpMethod request_get_method(RequestContext *ctx) {
    if (!ctx || !ctx->method) return HTTP_GET;
    if (strcmp(ctx->method, "POST") == 0) return HTTP_POST;
    if (strcmp(ctx->method, "PUT") == 0) return HTTP_PUT;
    if (strcmp(ctx->method, "DELETE") == 0) return HTTP_DELETE;
    if (strcmp(ctx->method, "PATCH") == 0) return HTTP_PATCH;
    if (strcmp(ctx->method, "OPTIONS") == 0) return HTTP_OPTIONS;
    return HTTP_GET;
}

const char *request_get_body(RequestContext *ctx) {
    return ctx ? ctx->body : "";
}

size_t request_get_body_length(RequestContext *ctx) {
    return ctx ? ctx->body_length : 0;
}

const char *request_get_query_param(RequestContext *ctx, const char *name) {
    if (!ctx || !name || !ctx->url) return "";
    const char *query = strchr(ctx->url, '?');
    if (!query) return "";
    query++;
    static char param_buf[256];
    const char *found = strstr(query, name);
    if (!found || (found != query && *(found - 1) != '&')) return "";
    found += strlen(name);
    if (*found != '=') return "";
    found++;
    size_t i = 0;
    while (*found && *found != '&' && i < sizeof(param_buf) - 1) {
        param_buf[i++] = *found++;
    }
    param_buf[i] = '\0';
    return param_buf;
}

const char *request_get_id(RequestContext *ctx) {
    return ctx ? ctx->request_id : "";
}

const char *request_get_client_ip(RequestContext *ctx) {
    return ctx ? ctx->client_ip : "";
}

const char *request_get_path_param(RequestContext *ctx, int index) {
    if (!ctx || index < 0 || index >= ctx->path_params.count) return "";
    return ctx->path_params.values[index] ? ctx->path_params.values[index] : "";
}

void *request_get_user_data(RequestContext *ctx) {
    if (!ctx) return NULL;
    return ctx->user_data;
}

/* ========================================================================
 * 响应实现
 * ======================================================================== */

void response_set_status(RequestContext *ctx, HttpStatus status) {
    if (ctx) ctx->status = status;
}

void response_set_header(RequestContext *ctx, const char *name, const char *value) {
    (void)ctx;
    (void)name;
    (void)value;
}

void response_set_json(RequestContext *ctx, const char *json_body) {
    if (!ctx) return;
    ctx->response_body = strdup(json_body ? json_body : "{}");
    strcpy(ctx->response_content_type, "application/json");
}

void response_set_text(RequestContext *ctx, const char *text) {
    if (!ctx) return;
    ctx->response_body = strdup(text ? text : "");
    strcpy(ctx->response_content_type, "text/plain; charset=utf-8");
}

void response_error(RequestContext *ctx, HttpStatus status, const char *code, const char *message) {
    if (!ctx) return;
    ctx->status = status;
    char json[512];
    snprintf(json, sizeof(json), "{\"error\":{\"code\":\"%s\",\"message\":\"%s\"}}",
             code ? code : "INTERNAL_ERROR", message ? message : "Unknown error");
    response_set_json(ctx, json);
}

/* ========================================================================
 * 路由注册
 * ======================================================================== */

int rest_server_register_handler(RestServer *server, HttpMethod method, const char *path_pattern,
                                  RequestHandler handler) {
    if (!server || !path_pattern || !handler) return -1;

    Route *route = malloc(sizeof(Route));
    route->method = method;
    route->path_pattern = strdup(path_pattern);
    route->handler = handler;
    route->next = server->routes;
    server->routes = route;

    return 0;
}

/* ========================================================================
 * 指标
 * ======================================================================== */

const char *rest_server_get_metrics(RestServer *server) {
    if (!server) return "";

    static char metrics[4096];
    int offset = 0;

    pthread_mutex_lock(&server->mutex);

    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "# HELP minivecdb_vectors_total Total number of vectors\n");
    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "# TYPE minivecdb_vectors_total gauge\n");
    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "minivecdb_vectors_total %lld\n", (long long)server->vector_count);

    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "# HELP minivecdb_collections_total Total number of collections\n");
    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "# TYPE minivecdb_collections_total gauge\n");
    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "minivecdb_collections_total %d\n", server->collection_count);

    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "# HELP minivecdb_request_total Total number of requests\n");
    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "# TYPE minivecdb_request_total counter\n");
    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "minivecdb_request_total{status=\"success\"} %llu\n",
        (unsigned long long)server->stats.success_requests);
    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "minivecdb_request_total{status=\"error\"} %llu\n",
        (unsigned long long)server->stats.error_requests);

    double avg_duration = server->stats.total_requests > 0 ?
        server->stats.total_duration_ms / server->stats.total_requests : 0;
    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "# HELP minivecdb_request_duration_avg Average request duration in ms\n");
    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "# TYPE minivecdb_request_duration_avg gauge\n");
    offset += snprintf(metrics + offset, sizeof(metrics) - offset,
        "minivecdb_request_duration_avg %.2f\n", avg_duration);

    pthread_mutex_unlock(&server->mutex);

    return metrics;
}

void rest_server_inc_vector_count(RestServer *server, int64_t delta) {
    if (!server) return;
    pthread_mutex_lock(&server->mutex);
    server->vector_count += delta;
    pthread_mutex_unlock(&server->mutex);
}

void rest_server_inc_collection_count(RestServer *server, int32_t delta) {
    if (!server) return;
    pthread_mutex_lock(&server->mutex);
    server->collection_count += delta;
    pthread_mutex_unlock(&server->mutex);
}

void rest_server_record_request(RestServer *server, const char *method, const char *path,
                                 int status, double duration_ms) {
    (void)server;
    (void)method;
    (void)path;
    (void)status;
    (void)duration_ms;
}
