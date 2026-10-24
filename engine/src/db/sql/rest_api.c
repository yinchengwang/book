/**
 * @file rest_api.c
 * @brief REST API 实现
 */

#include "db/sql/rest_api.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* ========================================================================
 * 工具函数
 * ======================================================================== */

const char *http_method_name(HttpMethod method)
{
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_PUT: return "PUT";
        case HTTP_PATCH: return "PATCH";
        case HTTP_DELETE: return "DELETE";
        case HTTP_OPTIONS: return "OPTIONS";
        case HTTP_HEAD: return "HEAD";
        default: return "UNKNOWN";
    }
}

const char *http_status_message(HttpStatus status)
{
    switch (status) {
        case HTTP_200_OK: return "OK";
        case HTTP_201_CREATED: return "Created";
        case HTTP_204_NO_CONTENT: return "No Content";
        case HTTP_400_BAD_REQUEST: return "Bad Request";
        case HTTP_401_UNAUTHORIZED: return "Unauthorized";
        case HTTP_403_FORBIDDEN: return "Forbidden";
        case HTTP_404_NOT_FOUND: return "Not Found";
        case HTTP_405_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case HTTP_409_CONFLICT: return "Conflict";
        case HTTP_422_UNPROCESSABLE_ENTITY: return "Unprocessable Entity";
        case HTTP_500_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case HTTP_501_NOT_IMPLEMENTED: return "Not Implemented";
        case HTTP_503_SERVICE_UNAVAILABLE: return "Service Unavailable";
        default: return "Unknown";
    }
}

char *http_url_decode(const char *str)
{
    if (!str) return NULL;

    char *result = malloc(strlen(str) + 1);
    int j = 0;

    for (int i = 0; str[i]; i++) {
        if (str[i] == '%' && isxdigit(str[i + 1]) && isxdigit(str[i + 2])) {
            char hex[3] = {str[i + 1], str[i + 2], '\0'};
            result[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (str[i] == '+') {
            result[j++] = ' ';
        } else {
            result[j++] = str[i];
        }
    }

    result[j] = '\0';
    return result;
}

char *http_url_encode(const char *str)
{
    if (!str) return NULL;

    char *result = malloc(strlen(str) * 3 + 1);
    int j = 0;

    for (int i = 0; str[i]; i++) {
        if (isalnum(str[i]) || str[i] == '-' || str[i] == '_' ||
            str[i] == '.' || str[i] == '~') {
            result[j++] = str[i];
        } else {
            snprintf(result + j, 4, "%%%02X", (unsigned char)str[i]);
            j += 3;
        }
    }

    result[j] = '\0';
    return result;
}

/* ========================================================================
 * JSON 实现
 * ======================================================================== */

JsonValue *json_string_create(const char *str)
{
    JsonValue *val = (JsonValue *)calloc(1, sizeof(JsonValue));
    if (!val) return NULL;

    val->type = JSON_STRING;
    val->val.string_val = str ? strdup(str) : strdup("");

    return val;
}

JsonValue *json_number_create(double num)
{
    JsonValue *val = (JsonValue *)calloc(1, sizeof(JsonValue));
    if (!val) return NULL;

    val->type = JSON_NUMBER;
    val->val.number_val = num;

    return val;
}

JsonValue *json_bool_create(bool val)
{
    JsonValue *v = (JsonValue *)calloc(1, sizeof(JsonValue));
    if (!v) return NULL;

    v->type = JSON_BOOL;
    v->val.bool_val = val;

    return v;
}

JsonArray *json_array_create(void)
{
    JsonArray *arr = (JsonArray *)calloc(1, sizeof(JsonArray));
    if (!arr) return NULL;

    arr->capacity = 16;
    arr->values = (JsonValue **)calloc(arr->capacity, sizeof(JsonValue *));

    return arr;
}

void json_array_push(JsonArray *arr, JsonValue *value)
{
    if (!arr || !value) return;

    if (arr->length >= arr->capacity) {
        arr->capacity *= 2;
        arr->values = (JsonValue **)realloc(arr->values,
                                            arr->capacity * sizeof(JsonValue *));
    }

    arr->values[arr->length++] = value;
}

JsonObject *json_object_create(void)
{
    JsonObject *obj = (JsonObject *)calloc(1, sizeof(JsonObject));
    if (!obj) return NULL;

    obj->capacity = 16;
    obj->keys = (char **)calloc(obj->capacity, sizeof(char *));
    obj->values = (JsonValue **)calloc(obj->capacity, sizeof(JsonValue *));

    return obj;
}

void json_object_set(JsonObject *obj, const char *key, JsonValue *value)
{
    if (!obj || !key || !value) return;

    /* 检查是否已存在 */
    for (int i = 0; i < obj->length; i++) {
        if (strcmp(obj->keys[i], key) == 0) {
            /* 替换 */
            obj->values[i] = value;
            return;
        }
    }

    /* 添加新字段 */
    if (obj->length >= obj->capacity) {
        obj->capacity *= 2;
        obj->keys = (char **)realloc(obj->keys, obj->capacity * sizeof(char *));
        obj->values = (JsonValue **)realloc(obj->values,
                                            obj->capacity * sizeof(JsonValue *));
    }

    obj->keys[obj->length] = strdup(key);
    obj->values[obj->length++] = value;
}

JsonValue *json_object_create_with(const char *key, JsonValue *value)
{
    JsonObject *obj = json_object_create();
    if (!obj) return NULL;

    json_object_set(obj, key, value);

    JsonValue *result = (JsonValue *)calloc(1, sizeof(JsonValue));
    result->type = JSON_OBJECT;
    result->val.object_val = obj;

    return result;
}

JsonValue *json_get(const JsonValue *json, const char *key)
{
    if (!json || json->type != JSON_OBJECT || !key) {
        return NULL;
    }

    JsonObject *obj = json->val.object_val;
    for (int i = 0; i < obj->length; i++) {
        if (strcmp(obj->keys[i], key) == 0) {
            return obj->values[i];
        }
    }

    return NULL;
}

JsonValue *json_array_get(const JsonValue *arr, int index)
{
    if (!arr || arr->type != JSON_ARRAY) {
        return NULL;
    }

    JsonArray *a = arr->val.array_val;
    if (index < 0 || index >= a->length) {
        return NULL;
    }

    return a->values[index];
}

void json_free(JsonValue *json)
{
    if (!json) return;

    switch (json->type) {
        case JSON_STRING:
            if (json->val.string_val) free(json->val.string_val);
            break;
        case JSON_ARRAY:
            if (json->val.array_val) {
                JsonArray *arr = json->val.array_val;
                for (int i = 0; i < arr->length; i++) {
                    json_free(arr->values[i]);
                }
                free(arr->values);
                free(arr);
            }
            break;
        case JSON_OBJECT:
            if (json->val.object_val) {
                JsonObject *obj = json->val.object_val;
                for (int i = 0; i < obj->length; i++) {
                    free(obj->keys[i]);
                    json_free(obj->values[i]);
                }
                free(obj->keys);
                free(obj->values);
                free(obj);
            }
            break;
        default:
            break;
    }

    free(json);
}

/**
 * @brief JSON 转字符串（简化实现）
 */
char *json_to_string(const JsonValue *json)
{
    if (!json) return strdup("null");

    switch (json->type) {
        case JSON_NULL:
            return strdup("null");
        case JSON_BOOL:
            return strdup(json->val.bool_val ? "true" : "false");
        case JSON_NUMBER:
            {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.15g", json->val.number_val);
                return strdup(buf);
            }
        case JSON_STRING:
            {
                char *encoded = http_url_encode(json->val.string_val);
                char *result = (char *)malloc(strlen(encoded) + 3);
                snprintf(result, strlen(encoded) + 3, "\"%s\"", encoded);
                free(encoded);
                return result;
            }
        case JSON_ARRAY:
            {
                JsonArray *arr = json->val.array_val;
                char *result = strdup("[");
                for (int i = 0; i < arr->length; i++) {
                    char *item = json_to_string(arr->values[i]);
                    result = (char *)realloc(result, strlen(result) + strlen(item) + 2);
                    strcat(result, item);
                    if (i < arr->length - 1) strcat(result, ",");
                    free(item);
                }
                result = (char *)realloc(result, strlen(result) + 2);
                strcat(result, "]");
                return result;
            }
        case JSON_OBJECT:
            {
                JsonObject *obj = json->val.object_val;
                char *result = strdup("{");
                for (int i = 0; i < obj->length; i++) {
                    char *key = http_url_encode(obj->keys[i]);
                    char *val = json_to_string(obj->values[i]);
                    result = (char *)realloc(result, strlen(result) + strlen(key) +
                                            strlen(val) + 5);
                    strcat(result, "\"");
                    strcat(result, key);
                    strcat(result, "\":");
                    strcat(result, val);
                    if (i < obj->length - 1) strcat(result, ",");
                    free(key);
                    free(val);
                }
                result = (char *)realloc(result, strlen(result) + 2);
                strcat(result, "}");
                return result;
            }
        default:
            return strdup("null");
    }
}

/**
 * @brief 解析 JSON（简化实现）
 */
JsonValue *json_parse(const char *str)
{
    if (!str) return NULL;

    /* 跳过空白 */
    while (*str && isspace(*str)) str++;

    if (*str == '{') {
        /* 对象 */
        JsonObject *obj = json_object_create();
        str++;
        while (*str && *str != '}') {
            while (*str && isspace(*str)) str++;
            if (*str != '"') break;
            str++;

            /* 解析键 */
            char key[256];
            int i = 0;
            while (*str && *str != '"' && i < sizeof(key) - 1) {
                key[i++] = *str++;
            }
            key[i] = '\0';
            if (*str == '"') str++;

            while (*str && isspace(*str)) str++;
            if (*str != ':') break;
            str++;

            /* 解析值 */
            while (*str && isspace(*str)) str++;
            JsonValue *val = json_parse(str);
            if (!val) break;

            json_object_set(obj, key, val);

            /* 跳过已解析的部分 */
            /* 简化：假设解析到下一个分隔符 */
            while (*str && *str != ',' && *str != '}') str++;
            if (*str == ',') str++;
        }
        if (*str == '}') str++;

        JsonValue *result = (JsonValue *)calloc(1, sizeof(JsonValue));
        result->type = JSON_OBJECT;
        result->val.object_val = obj;
        return result;
    } else if (*str == '[') {
        /* 数组 */
        JsonArray *arr = json_array_create();
        str++;
        while (*str && *str != ']') {
            while (*str && isspace(*str)) str++;
            if (*str == ']') break;

            JsonValue *val = json_parse(str);
            if (!val) break;

            json_array_push(arr, val);

            while (*str && *str != ',' && *str != ']') str++;
            if (*str == ',') str++;
        }
        if (*str == ']') str++;

        JsonValue *result = (JsonValue *)calloc(1, sizeof(JsonValue));
        result->type = JSON_ARRAY;
        result->val.array_val = arr;
        return result;
    } else if (*str == '"') {
        /* 字符串 */
        str++;
        char buf[4096];
        int i = 0;
        while (*str && *str != '"' && i < sizeof(buf) - 1) {
            buf[i++] = *str++;
        }
        buf[i] = '\0';
        if (*str == '"') str++;

        return json_string_create(buf);
    } else if (*str == '-' || isdigit(*str)) {
        /* 数字 */
        char buf[64];
        int i = 0;
        if (*str == '-') buf[i++] = *str++;
        while (*str && (isdigit(*str) || *str == '.' || *str == 'e' ||
                       *str == 'E' || *str == '+' || *str == '-')) {
            if (i < sizeof(buf) - 1) buf[i++] = *str;
            str++;
        }
        buf[i] = '\0';

        return json_number_create(atof(buf));
    } else if (strncmp(str, "true", 4) == 0) {
        str += 4;
        return json_bool_create(true);
    } else if (strncmp(str, "false", 5) == 0) {
        str += 5;
        return json_bool_create(false);
    } else if (strncmp(str, "null", 4) == 0) {
        str += 4;
        JsonValue *val = (JsonValue *)calloc(1, sizeof(JsonValue));
        val->type = JSON_NULL;
        return val;
    }

    return NULL;
}

/* ========================================================================
 * 请求解析
 * ======================================================================== */

void http_parse_query_params(HttpRequest *req, const char *query)
{
    if (!req || !query) return;

    /* 简化实现：存储原始查询 */
    strncpy(req->query, query, sizeof(req->query) - 1);
}

const char *http_get_query_param(const HttpRequest *req, const char *name)
{
    if (!req || !name) return NULL;

    /* 简化实现 */
    (void)req;
    (void)name;
    return NULL;
}

const char *http_get_path_param(const HttpRequest *req, const char *name)
{
    if (!req || !name) return NULL;

    /* 简化实现 */
    (void)req;
    (void)name;
    return NULL;
}

const char *http_get_header(const HttpRequest *req, const char *name)
{
    if (!req || !name) return NULL;

    if (strcmp(name, "Content-Type") == 0) return req->content_type;
    if (strcmp(name, "Authorization") == 0) return req->authorization;
    if (strcmp(name, "User-Agent") == 0) return req->user_agent;
    if (strcmp(name, "Accept") == 0) return req->accept;

    return NULL;
}

JsonValue *http_parse_json_body(const HttpRequest *req)
{
    if (!req || !req->body) return NULL;
    return json_parse(req->body);
}

/* ========================================================================
 * 响应构建
 * ======================================================================== */

HttpResponse *http_response_create(void)
{
    HttpResponse *res = (HttpResponse *)calloc(1, sizeof(HttpResponse));
    if (!res) return NULL;

    res->status = HTTP_200_OK;
    res->content_type = "application/json";
    res->no_cache = false;

    return res;
}

void http_response_destroy(HttpResponse *res)
{
    if (!res) return;

    if (res->body) free(res->body);
    /* TODO: 清理 headers */
    free(res);
}

void http_response_status(HttpResponse *res, HttpStatus status)
{
    if (res) res->status = status;
}

void http_response_json(HttpResponse *res, const JsonValue *json)
{
    if (!res) return;

    if (res->body) free(res->body);

    if (json) {
        res->body = json_to_string(json);
        res->body_len = strlen(res->body);
    } else {
        res->body = strdup("null");
        res->body_len = 4;
    }

    res->content_type = "application/json";
}

void http_response_text(HttpResponse *res, const char *text)
{
    if (!res) return;

    if (res->body) free(res->body);

    res->body = text ? strdup(text) : strdup("");
    res->body_len = strlen(res->body);
    res->content_type = "text/plain";
}

void http_response_error(HttpResponse *res, HttpStatus status,
                       const char *message)
{
    if (!res) return;

    JsonValue *error = json_object_create_with("error",
        json_string_create(message ? message : http_status_message(status)));
    http_response_json(res, error);
    res->status = status;
    json_free(error);
}

void http_response_header(HttpResponse *res, const char *name, const char *value)
{
    if (!res || !name || !value) return;
    /* TODO: 添加到 headers 列表 */
    (void)res;
}

/* ========================================================================
 * REST 服务器
 * ======================================================================== */

#define MAX_ROUTES 64

RestServer *rest_server_create(const RestConfig *config)
{
    RestServer *server = (RestServer *)calloc(1, sizeof(RestServer));
    if (!server) return NULL;

    if (config) {
        server->config = *config;
    } else {
        server->config.port = 8080;
        server->config.num_threads = 4;
        server->config.max_request_size = 1024 * 1024;
        server->config.max_header_size = 8192;
        server->config.request_timeout_ms = 30000;
    }

    server->routes = (Route *)calloc(MAX_ROUTES, sizeof(Route));
    server->n_routes = 0;
    server->max_routes = MAX_ROUTES;

    return server;
}

void rest_server_destroy(RestServer *server)
{
    if (!server) return;

    if (server->listen_fd >= 0) {
        close(server->listen_fd);
    }
    if (server->routes) free(server->routes);
    free(server);
}

int rest_server_route(RestServer *server, HttpMethod method,
                     const char *path, RouteHandler handler)
{
    if (!server || !path || !handler) return -1;
    if (server->n_routes >= server->max_routes) return -1;

    Route *route = &server->routes[server->n_routes++];
    route->method = method;
    route->path = path;
    route->handler = handler;

    return 0;
}

int rest_server_get(RestServer *server, const char *path,
                   RouteHandler handler)
{
    return rest_server_route(server, HTTP_GET, path, handler);
}

int rest_server_post(RestServer *server, const char *path,
                    RouteHandler handler)
{
    return rest_server_route(server, HTTP_POST, path, handler);
}

int rest_server_put(RestServer *server, const char *path,
                   RouteHandler handler)
{
    return rest_server_route(server, HTTP_PUT, path, handler);
}

int rest_server_delete(RestServer *server, const char *path,
                     RouteHandler handler)
{
    return rest_server_route(server, HTTP_DELETE, path, handler);
}

int rest_server_start(RestServer *server)
{
    struct sockaddr_in addr;

    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        return -1;
    }

    int opt = 1;
    setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->config.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server->listen_fd);
        return -1;
    }

    if (listen(server->listen_fd, 128) < 0) {
        close(server->listen_fd);
        return -1;
    }

    return 0;
}

void rest_server_stop(RestServer *server)
{
    if (server && server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }
}

/**
 * @brief 查找路由
 */
static Route *find_route(RestServer *server, HttpMethod method, const char *path)
{
    for (int i = 0; i < server->n_routes; i++) {
        Route *route = &server->routes[i];
        if (route->method == method &&
            strcmp(route->path, path) == 0) {
            return route;
        }
    }
    return NULL;
}

/**
 * @brief 处理请求（简化实现）
 */
int rest_server_handle(RestServer *server, int client_fd)
{
    char buf[8192];
    char method[16];
    char uri[512];
    char version[16];

    /* 读取请求行 */
    int n = read(client_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return -1;
    buf[n] = '\0';

    /* 解析请求行 */
    if (sscanf(buf, "%15s %511s %15s", method, uri, version) != 3) {
        return -1;
    }

    /* 创建请求 */
    HttpRequest req;
    memset(&req, 0, sizeof(req));

    if (strcmp(method, "GET") == 0) req.method = HTTP_GET;
    else if (strcmp(method, "POST") == 0) req.method = HTTP_POST;
    else if (strcmp(method, "PUT") == 0) req.method = HTTP_PUT;
    else if (strcmp(method, "DELETE") == 0) req.method = HTTP_DELETE;
    else req.method = HTTP_GET;

    strncpy(req.uri, uri, sizeof(req.uri) - 1);

    /* 解析路径 */
    char *path_end = strchr(uri, '?');
    if (path_end) {
        int path_len = path_end - uri;
        if (path_len >= sizeof(req.path)) path_len = sizeof(req.path) - 1;
        strncpy(req.path, uri, path_len);
        req.path[path_len] = '\0';
        strncpy(req.query, path_end + 1, sizeof(req.query) - 1);
    } else {
        strncpy(req.path, uri, sizeof(req.path) - 1);
    }

    /* 查找路由 */
    Route *route = find_route(server, req.method, req.path);

    /* 创建响应 */
    HttpResponse *res = http_response_create();

    if (route && route->handler) {
        route->handler(&req, res);
    } else {
        /* 路由未找到 */
        JsonValue *error = json_object_create_with("error",
            json_string_create("Not Found"));
        http_response_json(res, error);
        res->status = HTTP_404_NOT_FOUND;
        json_free(error);
    }

    /* 发送响应 */
    char status_line[64];
    snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n",
             res->status, http_status_message(res->status));

    char headers[512];
    snprintf(headers, sizeof(headers),
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             res->content_type, res->body_len);

    write(client_fd, status_line, strlen(status_line));
    write(client_fd, headers, strlen(headers));
    if (res->body && res->body_len > 0) {
        write(client_fd, res->body, res->body_len);
    }

    /* 清理 */
    http_response_destroy(res);
    close(client_fd);

    return 0;
}

/* ========================================================================
 * 示例路由处理器
 * ======================================================================== */

/**
 * @brief SQL 查询处理
 */
int handle_query(HttpRequest *req, HttpResponse *res)
{
    /* TODO: 调用 SQL 执行器 */
    JsonValue *body = http_parse_json_body(req);
    if (!body) {
        http_response_error(res, HTTP_400_BAD_REQUEST, "Invalid JSON");
        return 0;
    }

    JsonValue *sql = json_get(body, "sql");
    if (!sql || sql->type != JSON_STRING) {
        http_response_error(res, HTTP_400_BAD_REQUEST, "Missing or invalid 'sql' field");
        json_free(body);
        return 0;
    }

    /* TODO: 执行查询并返回结果 */
    JsonValue *result = json_object_create();
    json_object_set(result, "rows", json_array_create());
    json_object_set(result, "affected_rows", json_number_create(0));

    http_response_json(res, result);
    json_free(body);
    json_free(result);
    return 0;
}

/**
 * @brief 集合列表处理
 */
int handle_collections(HttpRequest *req, HttpResponse *res)
{
    if (req->method == HTTP_GET) {
        /* 返回集合列表 */
        JsonValue *result = json_object_create();
        JsonArray *arr = json_array_create();
        json_object_set(result, "collections", arr);
        json_object_set(result, "total", json_number_create(0));
        http_response_json(res, result);
        json_free(result);
    } else if (req->method == HTTP_POST) {
        /* 创建集合 */
        JsonValue *body = http_parse_json_body(req);
        if (!body) {
            http_response_error(res, HTTP_400_BAD_REQUEST, "Invalid JSON");
            return 0;
        }

        JsonValue *name = json_get(body, "name");
        if (!name || name->type != JSON_STRING) {
            http_response_error(res, HTTP_400_BAD_REQUEST, "Missing 'name' field");
            json_free(body);
            return 0;
        }

        /* TODO: 创建集合 */
        JsonValue *result = json_object_create_with("id",
            json_string_create("test-collection"));
        http_response_json(res, result);
        json_free(body);
        json_free(result);
    }

    return 0;
}

/**
 * @brief 向量搜索处理
 */
int handle_vector_search(HttpRequest *req, HttpResponse *res)
{
    JsonValue *body = http_parse_json_body(req);
    if (!body) {
        http_response_error(res, HTTP_400_BAD_REQUEST, "Invalid JSON");
        return 0;
    }

    JsonValue *collection = json_get(body, "collection");
    JsonValue *query = json_get(body, "query");
    JsonValue *top_k = json_get(body, "top_k");

    if (!collection || !query) {
        http_response_error(res, HTTP_400_BAD_REQUEST, "Missing required fields");
        json_free(body);
        return 0;
    }

    /* TODO: 执行向量搜索 */
    JsonValue *result = json_object_create();
    JsonArray *arr = json_array_create();
    json_object_set(result, "results", arr);
    json_object_set(result, "query", query);
    json_object_set(result, "top_k", top_k ? top_k : json_number_create(10));

    http_response_json(res, result);
    json_free(body);
    json_free(result);
    return 0;
}

/**
 * @brief 健康检查处理
 */
int handle_health(HttpRequest *req, HttpResponse *res)
{
    (void)req;

    JsonValue *result = json_object_create();
    json_object_set(result, "status", json_string_create("healthy"));
    json_object_set(result, "version", json_string_create("1.0.0"));

    http_response_json(res, result);
    json_free(result);
    return 0;
}

/**
 * @brief 指标处理
 */
int handle_metrics(HttpRequest *req, HttpResponse *res)
{
    (void)req;

    JsonValue *result = json_object_create();
    json_object_set(result, "queries_total", json_number_create(0));
    json_object_set(result, "queries_per_second", json_number_create(0.0));
    json_object_set(result, "active_connections", json_number_create(0));
    json_object_set(result, "total_collections", json_number_create(0));
    json_object_set(result, "total_vectors", json_number_create(0));

    http_response_json(res, result);
    json_free(result);
    return 0;
}

/**
 * @brief 设置默认路由
 */
void rest_server_set_default_routes(RestServer *server)
{
    rest_server_post(server, "/api/v1/query", handle_query);
    rest_server_get(server, "/api/v1/collections", handle_collections);
    rest_server_post(server, "/api/v1/collections", handle_collections);
    rest_server_post(server, "/api/v1/vectors/search", handle_vector_search);
    rest_server_get(server, "/api/v1/health", handle_health);
    rest_server_get(server, "/api/v1/metrics", handle_metrics);
}
