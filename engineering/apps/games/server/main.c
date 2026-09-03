#include <stdio.h>
#include <stdlib.h>
#include <microhttpd.h>

#define PORT 8080

static int
handle_request(void *cls, struct MHD_Connection *connection,
               const char *url, const char *method,
               const char *version, const char *upload_data,
               size_t *upload_data_size, void **con_cls) {
    (void)cls; (void)version; (void)upload_data; (void)upload_data_size; (void)con_cls;

    /* TODO: 路由分发
     *   POST /game/new?type=snake|2048  → 创建游戏实例
     *   POST /game/{id}/input           → 提交方向输入
     *   GET  /game/{id}/state           → 获取棋盘快照
     */
    const char *page = "<html><body><h1>游戏服务器</h1><p>TODO: 实现 REST API</p></body></html>";
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(page), (void*)page, MHD_RESPMEM_PERSISTENT);
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    struct MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
        &handle_request, NULL, MHD_OPTION_END);
    if (daemon == NULL) {
        fprintf(stderr, "启动服务器失败\n");
        return 1;
    }
    printf("游戏服务器运行于 http://localhost:%d\n", PORT);
    printf("按 Enter 停止...\n");
    getchar();
    MHD_stop_daemon(daemon);
    return 0;
}
