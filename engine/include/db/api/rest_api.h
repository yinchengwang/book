/**
 * @file rest_api.h
 * @brief MiniVecDB REST API 头文件
 *
 * 基于 libmicrohttpd 的 HTTP REST API 服务器实现。
 *
 * API 端点:
 * - GET  /health          健康检查
 * - GET  /ready           就绪检查
 * - GET  /live            活跃检查
 * - GET  /metrics         Prometheus 指标
 * - POST /collections     创建集合
 * - GET  /collections     列出所有集合
 * - GET  /collections/:id 获取集合详情
 * - DELETE /collections/:id 删除集合
 * - POST /collections/:id/vectors    批量插入向量
 * - POST /collections/:id/search     向量搜索
 * - GET  /collections/:id/vectors/:vid 获取向量
 * - PUT  /collections/:id/vectors/:vid 更新向量
 * - DELETE /collections/:id/vectors/:vid 删除向量
 */
#ifndef DB_REST_API_H
#define DB_REST_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/** REST API 服务器句柄（不透明） */
typedef struct RestServer_s RestServer;

/** HTTP 方法 */
typedef enum HttpMethod_e {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_PATCH,
    HTTP_OPTIONS
} HttpMethod;

/** HTTP 状态码 */
typedef enum HttpStatus_e {
    HTTP_OK = 200,
    HTTP_CREATED = 201,
    HTTP_NO_CONTENT = 204,
    HTTP_BAD_REQUEST = 400,
    HTTP_NOT_FOUND = 404,
    HTTP_INTERNAL_ERROR = 500,
    HTTP_SERVICE_UNAVAILABLE = 503
} HttpStatus;

/** 请求上下文 */
typedef struct RequestContext_s RequestContext;

/** 请求处理器函数类型 */
typedef int (*RequestHandler)(RequestContext *ctx);

/** 关闭钩子函数类型 */
typedef void (*ShutdownHook)(void *arg);

/* ========================================================================
 * 服务器配置
 * ======================================================================== */

/** 服务器配置 */
typedef struct RestServerConfig_s {
    const char *host;              /**< 监听地址，默认 0.0.0.0 */
    int port;                      /**< 监听端口，默认 8080 */
    const char *data_dir;          /**< 数据目录 */
    int max_request_size;          /**< 最大请求大小，默认 64MB */
    int connection_timeout;        /**< 连接超时（秒），默认 30 */
    int shutdown_timeout;          /**< 关闭超时（秒），默认 30 */
    bool enable_cors;              /**< 是否启用 CORS，默认 false */
} RestServerConfig;

/**
 * @brief 获取默认服务器配置
 * @return 默认配置
 */
RestServerConfig rest_server_default_config(void);

/* ========================================================================
 * 服务器生命周期
 * ======================================================================== */

/**
 * @brief 获取服务器内 BM25 索引映射（供 handlers 使用）
 * @param server 服务器句柄
 * @return BM25 索引哈希表指针（不透明，仅供 handlers.c 内部使用）
 */
void *rest_server_get_bm25_map(RestServer *server);

/**
 * @brief 获取服务器内关联的 VectorAPI
 */
void *rest_server_get_vector_api(RestServer *server);

/**
 * @brief 获取服务器内关联的 VDB 统一 API 句柄
 */
void *rest_server_get_vdb(RestServer *server);

/**
 * @brief 查找集合关联的 BM25 索引（不存在返回 NULL）
 */
void *rest_server_bm25_lookup(RestServer *server, const char *name);

/**
 * @brief 获取/创建集合关联的 BM25 索引
 */
void *rest_server_bm25_ensure(RestServer *server, const char *name);

/**
 * @brief 删除集合关联的 BM25 索引
 */
void rest_server_bm25_drop(RestServer *server, const char *name);

/**
 * @brief 创建 REST API 服务器
 * @param config 服务器配置
 * @return 服务器句柄，失败返回 NULL
 */
RestServer *rest_server_create(const RestServerConfig *config);

/**
 * @brief 启动服务器
 * @param server 服务器句柄
 * @return 0 成功，负数失败
 */
int rest_server_start(RestServer *server);

/**
 * @brief 停止服务器
 * @param server 服务器句柄
 */
void rest_server_stop(RestServer *server);

/**
 * @brief 等待服务器关闭
 * @param server 服务器句柄
 */
void rest_server_wait(RestServer *server);

/**
 * @brief 销毁服务器
 * @param server 服务器句柄
 */
void rest_server_destroy(RestServer *server);

/**
 * @brief 获取服务器运行状态
 * @param server 服务器句柄
 * @return true 运行中，false 已停止
 */
bool rest_server_is_running(RestServer *server);

/**
 * @brief 获取服务器启动时间（秒）
 * @param server 服务器句柄
 * @return 启动时间戳，0 表示未启动
 */
int64_t rest_server_get_uptime(RestServer *server);

/* ========================================================================
 * 关闭钩子
 * ======================================================================== */

/**
 * @brief 注册关闭钩子
 * @param server 服务器句柄
 * @param hook 关闭回调函数
 * @param arg 传递给回调的参数
 * @return 0 成功，负数失败
 */
int rest_server_register_shutdown_hook(RestServer *server, ShutdownHook hook, void *arg);

/* ========================================================================
 * 请求上下文
 * ======================================================================== */

/**
 * @brief 获取请求路径
 * @param ctx 请求上下文
 * @return 请求路径字符串
 */
const char *request_get_path(RequestContext *ctx);

/**
 * @brief 获取请求方法
 * @param ctx 请求上下文
 * @return HTTP 方法
 */
HttpMethod request_get_method(RequestContext *ctx);

/**
 * @brief 获取请求体
 * @param ctx 请求上下文
 * @return 请求体字符串，NULL 如果没有
 */
const char *request_get_body(RequestContext *ctx);

/**
 * @brief 获取请求体长度
 * @param ctx 请求上下文
 * @return 请求体长度
 */
size_t request_get_body_length(RequestContext *ctx);

/**
 * @brief 获取查询参数
 * @param ctx 请求上下文
 * @param name 参数名
 * @return 参数值，NULL 如果不存在
 */
const char *request_get_query_param(RequestContext *ctx, const char *name);

/**
 * @brief 获取请求 ID
 * @param ctx 请求上下文
 * @return 请求 ID
 */
const char *request_get_id(RequestContext *ctx);

/**
 * @brief 获取客户端 IP
 * @param ctx 请求上下文
 * @return 客户端 IP 地址
 */
const char *request_get_client_ip(RequestContext *ctx);

/**
 * @brief 获取路径参数
 * @param ctx 请求上下文
 * @param index 参数索引（从 0 开始）
 * @return 参数值，NULL 如果不存在
 */
const char *request_get_path_param(RequestContext *ctx, int index);

/**
 * @brief 获取用户自定义数据（指向 server->vector_api 等）
 * @param ctx 请求上下文
 * @return user_data 指针，NULL 如果未设置
 */
void *request_get_user_data(RequestContext *ctx);

/* ========================================================================
 * 响应
 * ======================================================================== */

/**
 * @brief 设置响应状态码
 * @param ctx 请求上下文
 * @param status HTTP 状态码
 */
void response_set_status(RequestContext *ctx, HttpStatus status);

/**
 * @brief 设置响应头
 * @param ctx 请求上下文
 * @param name 头名称
 * @param value 头值
 */
void response_set_header(RequestContext *ctx, const char *name, const char *value);

/**
 * @brief 设置响应体（JSON）
 * @param ctx 请求上下文
 * @param json_body JSON 字符串
 */
void response_set_json(RequestContext *ctx, const char *json_body);

/**
 * @brief 设置纯文本响应
 * @param ctx 请求上下文
 * @param text 文本内容
 */
void response_set_text(RequestContext *ctx, const char *text);

/**
 * @brief 发送错误响应
 * @param ctx 请求上下文
 * @param status HTTP 状态码
 * @param code 错误码
 * @param message 错误消息
 */
void response_error(RequestContext *ctx, HttpStatus status, const char *code, const char *message);

/* ========================================================================
 * 路由注册
 * ======================================================================== */

/**
 * @brief 注册请求处理器
 * @param server 服务器句柄
 * @param method HTTP 方法
 * @param path_pattern 路径模式（如 "/collections/:id"）
 * @param handler 请求处理函数
 * @return 0 成功，负数失败
 */
int rest_server_register_handler(RestServer *server, HttpMethod method, const char *path_pattern,
                                  RequestHandler handler);

/* ========================================================================
 * 处理器注册
 * ======================================================================== */

/**
 * @brief 注册所有 REST API 处理器
 * @param server 服务器句柄
 */
void rest_api_register_handlers(RestServer *server);

/* ========================================================================
 * 指标
 * ======================================================================== */

/**
 * @brief 获取服务器指标（Prometheus 格式）
 * @param server 服务器句柄
 * @return Prometheus 格式的指标字符串
 */
const char *rest_server_get_metrics(RestServer *server);

/**
 * @brief 更新向量计数
 * @param server 服务器句柄
 * @param delta 变化量（正数增加，负数减少）
 */
void rest_server_inc_vector_count(RestServer *server, int64_t delta);

/**
 * @brief 更新集合计数
 * @param server 服务器句柄
 * @param delta 变化量
 */
void rest_server_inc_collection_count(RestServer *server, int32_t delta);

/**
 * @brief 记录请求
 * @param server 服务器句柄
 * @param method HTTP 方法
 * @param path 请求路径
 * @param status 响应状态码
 * @param duration_ms 处理时长（毫秒）
 */
void rest_server_record_request(RestServer *server, const char *method, const char *path,
                                 int status, double duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* DB_REST_API_H */
