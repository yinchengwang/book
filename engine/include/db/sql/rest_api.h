/**
 * @file rest_api.h
 * @brief REST API 头文件
 *
 * 实现轻量级 REST API：
 * 1. HTTP 服务器
 * 2. 路由分发
 * 3. SQL 查询接口
 * 4. 集合管理
 * 5. 向量搜索
 */
#ifndef DB_REST_API_H
#define DB_REST_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * HTTP 方法和状态码
 * ======================================================================== */

/** HTTP 方法 */
typedef enum HttpMethod_e {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_PATCH,
    HTTP_DELETE,
    HTTP_OPTIONS,
    HTTP_HEAD
} HttpMethod;

/** HTTP 状态码 */
typedef enum HttpStatus_e {
    HTTP_200_OK = 200,
    HTTP_201_CREATED = 201,
    HTTP_204_NO_CONTENT = 204,
    HTTP_400_BAD_REQUEST = 400,
    HTTP_401_UNAUTHORIZED = 401,
    HTTP_403_FORBIDDEN = 403,
    HTTP_404_NOT_FOUND = 404,
    HTTP_405_METHOD_NOT_ALLOWED = 405,
    HTTP_409_CONFLICT = 409,
    HTTP_422_UNPROCESSABLE_ENTITY = 422,
    HTTP_500_INTERNAL_SERVER_ERROR = 500,
    HTTP_501_NOT_IMPLEMENTED = 501,
    HTTP_503_SERVICE_UNAVAILABLE = 503
} HttpStatus;

/* ========================================================================
 * HTTP 请求
 * ======================================================================== */

/** HTTP 请求 */
typedef struct HttpRequest_s {
    HttpMethod method;             /**< HTTP 方法 */
    char uri[512];               /**< 请求 URI */
    char path[256];              /**< 路径 */
    char query[512];             /**< 查询字符串 */
    char *body;                  /**< 请求体 */
    int body_len;                /**< 请求体长度 */
    char *content_type;          /**< Content-Type */
    char *authorization;        /**< Authorization 头 */
    char *user_agent;            /**< User-Agent */
    char *accept;                /**< Accept 头 */
    char remote_addr[64];        /**< 客户端地址 */
    int remote_port;             /**< 客户端端口 */
    void *headers;              /**< 所有头部（键值对） */
    void *params;              /**< 路径参数 */
    void *query_params;        /**< 查询参数 */
} HttpRequest;

/** 查询参数 */
typedef struct QueryParam_s {
    char *key;
    char *value;
} QueryParam;

/* ========================================================================
 * HTTP 响应
 * ======================================================================== */

/** HTTP 响应 */
typedef struct HttpResponse_s {
    HttpStatus status;            /**< 状态码 */
    char *content_type;          /**< Content-Type */
    char *body;                 /**< 响应体 */
    int body_len;               /**< 响应体长度 */
    void *headers;              /**< 自定义头部 */
    bool no_cache;              /**< 禁用缓存 */
} HttpResponse;

/* ========================================================================
 * 路由
 * ======================================================================== */

/** 路由处理函数 */
typedef int (*RouteHandler)(HttpRequest *req, HttpResponse *res);

/** 路由 */
typedef struct Route_s {
    HttpMethod method;            /**< HTTP 方法 */
    const char *path;            /**< 路径模板 */
    RouteHandler handler;         /**< 处理函数 */
    void *user_data;            /**< 用户数据 */
} Route;

/* ========================================================================
 * REST API 服务器
 * ======================================================================== */

/** REST API 配置 */
typedef struct RestConfig_s {
    int port;                    /**< 监听端口 */
    int num_threads;            /**< 工作线程数 */
    int max_request_size;       /**< 最大请求大小 */
    int max_header_size;        /**< 最大头部大小 */
    int request_timeout_ms;     /**< 请求超时 */
    const char *cors_origin;    /**< CORS 源 */
    const char *api_key;        /**< API 密钥 */
} RestConfig;

/** REST API 服务器 */
typedef struct RestServer_s {
    int listen_fd;               /**< 监听 fd */
    RestConfig config;          /**< 配置 */
    Route *routes;              /**< 路由表 */
    int n_routes;               /**< 路由数 */
    int max_routes;            /**< 最大路由数 */
    void *server_ctx;          /**< 服务器上下文 */
} RestServer;

/* ========================================================================
 * JSON 工具
 * ======================================================================== */

/** JSON 值类型 */
typedef enum JsonType_e {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

/** JSON 值 */
typedef struct JsonValue_s {
    JsonType type;
    union {
        bool bool_val;
        double number_val;
        char *string_val;
        struct JsonArray_s *array_val;
        struct JsonObject_s *object_val;
    } val;
} JsonValue;

/** JSON 数组 */
typedef struct JsonArray_s {
    JsonValue **values;
    int length;
    int capacity;
} JsonArray;

/** JSON 对象 */
typedef struct JsonObject_s {
    char **keys;
    JsonValue **values;
    int length;
    int capacity;
} JsonObject;

/**
 * @brief 解析 JSON 字符串
 */
JsonValue *json_parse(const char *str);

/**
 * @brief 释放 JSON 值
 */
void json_free(JsonValue *json);

/**
 * @brief 将 JSON 转换为字符串
 */
char *json_to_string(const JsonValue *json);

/**
 * @brief 获取 JSON 对象字段
 */
JsonValue *json_get(const JsonValue *json, const char *key);

/**
 * @brief 获取 JSON 数组元素
 */
JsonValue *json_array_get(const JsonValue *arr, int index);

/**
 * @brief 创建 JSON 对象
 */
JsonObject *json_object_create(void);

/**
 * @brief 添加字段到 JSON 对象
 */
void json_object_set(JsonObject *obj, const char *key, JsonValue *value);

/**
 * @brief 创建 JSON 字符串
 */
JsonValue *json_string_create(const char *str);

/**
 * @brief 创建 JSON 数字
 */
JsonValue *json_number_create(double num);

/**
 * @brief 创建 JSON 布尔
 */
JsonValue *json_bool_create(bool val);

/**
 * @brief 创建 JSON 数组
 */
JsonArray *json_array_create(void);

/**
 * @brief 添加元素到 JSON 数组
 */
void json_array_push(JsonArray *arr, JsonValue *value);

/* ========================================================================
 * 请求解析
 * ======================================================================== */

/**
 * @brief 解析查询参数
 */
void http_parse_query_params(HttpRequest *req, const char *query);

/**
 * @brief 获取查询参数
 */
const char *http_get_query_param(const HttpRequest *req, const char *name);

/**
 * @brief 获取路径参数
 */
const char *http_get_path_param(const HttpRequest *req, const char *name);

/**
 * @brief 获取请求头
 */
const char *http_get_header(const HttpRequest *req, const char *name);

/**
 * @brief 解析 JSON 请求体
 */
JsonValue *http_parse_json_body(const HttpRequest *req);

/* ========================================================================
 * 响应构建
 * ======================================================================== */

/**
 * @brief 创建响应
 */
HttpResponse *http_response_create(void);

/**
 * @brief 销毁响应
 */
void http_response_destroy(HttpResponse *res);

/**
 * @brief 设置响应状态
 */
void http_response_status(HttpResponse *res, HttpStatus status);

/**
 * @brief 设置 JSON 响应体
 */
void http_response_json(HttpResponse *res, const JsonValue *json);

/**
 * @brief 设置文本响应体
 */
void http_response_text(HttpResponse *res, const char *text);

/**
 * @brief 设置错误响应
 */
void http_response_error(HttpResponse *res, HttpStatus status,
                        const char *message);

/**
 * @brief 添加响应头
 */
void http_response_header(HttpResponse *res, const char *name,
                         const char *value);

/* ========================================================================
 * REST API 服务器
 * ======================================================================== */

/**
 * @brief 创建 REST API 服务器
 */
RestServer *rest_server_create(const RestConfig *config);

/**
 * @brief 销毁服务器
 */
void rest_server_destroy(RestServer *server);

/**
 * @brief 注册路由
 */
int rest_server_route(RestServer *server, HttpMethod method,
                     const char *path, RouteHandler handler);

/**
 * @brief 注册 GET 路由
 */
int rest_server_get(RestServer *server, const char *path,
                   RouteHandler handler);

/**
 * @brief 注册 POST 路由
 */
int rest_server_post(RestServer *server, const char *path,
                    RouteHandler handler);

/**
 * @brief 注册 PUT 路由
 */
int rest_server_put(RestServer *server, const char *path,
                   RouteHandler handler);

/**
 * @brief 注册 DELETE 路由
 */
int rest_server_delete(RestServer *server, const char *path,
                      RouteHandler handler);

/**
 * @brief 启动服务器
 */
int rest_server_start(RestServer *server);

/**
 * @brief 停止服务器
 */
void rest_server_stop(RestServer *server);

/**
 * @brief 处理请求
 */
int rest_server_handle(RestServer *server, int client_fd);

/**
 * @brief 设置请求处理函数
 */
typedef int (*RequestHandler)(HttpRequest *req, HttpResponse *res);
void rest_server_set_handler(RestServer *server, RequestHandler handler);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief URL 解码
 */
char *http_url_decode(const char *str);

/**
 * @brief URL 编码
 */
char *http_url_encode(const char *str);

/**
 * @brief 获取 HTTP 方法名称
 */
const char *http_method_name(HttpMethod method);

/**
 * @brief 获取状态码描述
 */
const char *http_status_message(HttpStatus status);

#ifdef __cplusplus
}
#endif

#endif /* DB_REST_API_H */
