#ifndef TODO_HANDLER_H
#define TODO_HANDLER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Todo 处理器
 */
void todo_handler_init(void);

/**
 * @brief 启动 HTTP 服务器（阻塞）
 * @param port 监听端口
 */
int http_server_start(int port);

/**
 * @brief 停止 HTTP 服务器
 */
void http_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* TODO_HANDLER_H */
