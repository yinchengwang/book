/**
 * @file include/db/ecosystem/rest_server.h
 * @brief Ecosystem REST API 服务器公共 API
 */
#ifndef DB_ECOSYSTEM_REST_SERVER_H
#define DB_ECOSYSTEM_REST_SERVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** REST 服务器句柄（不透明） */
typedef struct rest_server rest_server_t;

/* ============================================================
 * 生命周期
 * ============================================================ */

/**
 * @brief 创建 REST API 服务器
 * @param port 监听端口
 * @return 服务器句柄，失败返回 NULL
 */
rest_server_t *rest_server_create(int port);

/**
 * @brief 启动 REST API 服务器
 * @param srv 服务器句柄
 * @return 0 成功，-1 失败
 */
int rest_server_start(rest_server_t *srv);

/**
 * @brief 停止 REST API 服务器
 * @param srv 服务器句柄
 */
void rest_server_stop(rest_server_t *srv);

/**
 * @brief 销毁 REST API 服务器
 * @param srv 服务器句柄
 */
void rest_server_destroy(rest_server_t *srv);

#ifdef __cplusplus
}
#endif

#endif /* DB_ECOSYSTEM_REST_SERVER_H */
