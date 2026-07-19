/**
 * @file db_server.c
 * @brief 数据库服务器实现 - 委托给 pgwire.c（Task 3.7）
 *
 * 保留 db_server.h 的外部 API 不变（server_start, server_stop 等），
 * 内部实现委托给 pgwire.c 库。
 */

#include "db/db_server.h"
#include "db/sql/pgwire.h"
#include "db/log.h"
#include "db/errors.h"
#include "db/guc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#define close closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

/* ============================================================
 * 静态变量
 * ============================================================ */

/** pgwire 服务器实例 */
static PgwireServer *g_pgwire_server = NULL;

/** 监听线程（Windows 用 HANDLE，POSIX 用 pthread_t） */
#ifdef _WIN32
static HANDLE g_listen_thread = NULL;
#else
static pthread_t g_listen_thread;
#endif

static bool g_running = false;

/* ============================================================
 * SQL 执行回调
 * ============================================================ */

/**
 * @brief pgwire 的 SQL 执行回调
 *
 * 将 pgwire 收到的 SQL 查询转发到实际的 SQL 执行器。
 * 当前为简化实现：返回空结果集。
 */
static int exec_sql_callback(PgwireSession *session, const char *query,
                             void **result)
{
    (void)session;
    (void)result;
    LOG_INFO("执行 SQL: %s", query);
    return 0;
}

/* ============================================================
 * 认证回调
 * ============================================================ */

static int auth_callback(PgwireConn *conn, const char *user,
                         const char *password)
{
    (void)conn;
    (void)password;
    LOG_INFO("认证: user=%s", user);
    return 0;  /* 信任所有连接 */
}

/* ============================================================
 * 参数回调
 * ============================================================ */

static const char *param_callback(PgwireSession *session, const char *name)
{
    (void)session;
    if (strcmp(name, "server_version") == 0) return "16.0 (self-made)";
    if (strcmp(name, "server_encoding") == 0) return "UTF8";
    if (strcmp(name, "client_encoding") == 0) return "UTF8";
    if (strcmp(name, "DateStyle") == 0) return "ISO";
    return "";
}

/* ============================================================
 * 公共 API
 * ============================================================ */

int server_start(int port, const char *data_dir)
{
    if (g_running) {
        LOG_WARN("服务器已在运行");
        return 0;
    }

    /* 创建 pgwire 服务器配置 */
    PgwireServerConfig config;
    memset(&config, 0, sizeof(config));
    config.port = port;
    config.max_connections = MAX_CONNECTIONS;

    /* 创建 pgwire 服务器 */
    g_pgwire_server = pgwire_server_create(&config);
    if (!g_pgwire_server) {
        LOG_ERROR("创建 pgwire 服务器失败");
        return -1;
    }

    /* 设置回调 */
    pgwire_server_set_auth_callback(g_pgwire_server, auth_callback);
    pgwire_server_set_exec_callback(g_pgwire_server, exec_sql_callback);
    pgwire_server_set_param_callback(g_pgwire_server, param_callback);

    /* 启动 pgwire 服务器 */
    if (pgwire_server_start(g_pgwire_server) != 0) {
        LOG_ERROR("pgwire 服务器启动失败（port=%d）", port);
        pgwire_server_destroy(g_pgwire_server);
        g_pgwire_server = NULL;
        return -1;
    }

    g_running = true;

    LOG_INFO("服务器启动: port=%d data_dir=%s", port, data_dir ? data_dir : "(null)");

    return 0;
}

void server_stop(void)
{
    LOG_INFO("服务器停止...");
    g_running = false;

    if (g_pgwire_server) {
        pgwire_server_stop(g_pgwire_server);
        pgwire_server_destroy(g_pgwire_server);
        g_pgwire_server = NULL;
    }
}

void server_run(void)
{
    if (!g_pgwire_server || !g_running) return;

    LOG_INFO("服务器主循环启动");

    while (g_running) {
        /* 接受新连接 */
        PgwireConn *conn = pgwire_server_accept(g_pgwire_server);
        if (!conn) continue;

        LOG_INFO("收到新连接 (fd=%d)", conn->fd);

        /* 创建会话并处理 */
        PgwireSession *session = pgwire_session_create(conn);
        if (session) {
            pgwire_conn_process(conn);
            pgwire_session_destroy(session);
        }

        pgwire_conn_destroy(conn);
    }
}

int server_execute_sql(const char *sql, char *output, size_t output_len)
{
    (void)sql;
    (void)output;
    (void)output_len;
    LOG_INFO("执行 SQL (stub): %s", sql);
    return 0;
}

/* ============================================================
 * 主函数
 * ============================================================ */

#ifdef DB_SERVER_MAIN
int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    const char *data_dir = NULL;
    const char *config_file = NULL;

    /* 解析参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-D") == 0 && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            config_file = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [OPTIONS]\n", argv[0]);
            printf("  -D DIR      数据目录\n");
            printf("  -p PORT     端口 (默认 %d)\n", DEFAULT_PORT);
            printf("  -c FILE     配置文件\n");
            return 0;
        }
    }

    /* 加载配置 */
    if (config_file) {
        guc_init(data_dir);
        guc_load_file(config_file);
    } else if (data_dir) {
        guc_init(data_dir);
        char conf[256];
        snprintf(conf, sizeof(conf), "%s/postgresql.conf", data_dir);
        guc_load_file(conf);
    }

    /* 启动服务器 */
    if (server_start(port, data_dir) != 0) {
        fprintf(stderr, "服务器启动失败\n");
        return 1;
    }

    printf("数据库服务器已启动，按 Ctrl+C 停止...\n");

    /* 主循环 */
    server_run();

    server_stop();
    guc_shutdown();
    return 0;
}
#endif