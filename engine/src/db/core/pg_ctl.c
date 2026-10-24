/**
 * @file pg_ctl.c
 * @brief pg_ctl 工具实现
 */

#include "db/pg_ctl.h"
#include "db/initdb.h"
#include "db/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#define getpid() _getpid()
#else
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

/* ============================================================
 * 常量
 * ============================================================ */

#define PG_CTL_VERSION "1.0.0"
#define MAX_PATH_LEN 1024
#define MAX_LINE_LEN 256

/** PID 文件名 */
#define PID_FILE "postmaster.pid"

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 获取 PID 文件路径
 */
static void get_pid_path(const char *data_dir, char *path, size_t len) {
    snprintf(path, len, "%s/" PID_FILE, data_dir);
}

/**
 * @brief 读取 PID 文件
 */
static int read_pid_file(const char *data_dir) {
    char path[MAX_PATH_LEN];
    char line[MAX_LINE_LEN];
    FILE *f;
    int pid = -1;

    get_pid_path(data_dir, path, sizeof(path));
    f = fopen(path, "r");
    if (!f) return -1;

    if (fgets(line, sizeof(line), f)) {
        pid = atoi(line);
    }
    fclose(f);

    return pid;
}

/**
 * @brief 检查进程是否存在
 */
static bool process_exists(int pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid);
    if (h) {
        CloseHandle(h);
        return true;
    }
    return false;
#else
    return kill(pid, 0) == 0;
#endif
}

/**
 * @brief 检查服务器是否运行
 */
static bool server_running(const char *data_dir) {
    int pid = read_pid_file(data_dir);
    if (pid <= 0) return false;
    return process_exists(pid);
}

/**
 * @brief 写入 PID 文件
 */
static int write_pid_file(const char *data_dir) {
    char path[MAX_PATH_LEN];
    FILE *f;
    time_t now;
    char time_buf[64];

    get_pid_path(data_dir, path, sizeof(path));
    f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("无法写入 PID 文件: %s", path);
        return -1;
    }

    time(&now);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S %Z", localtime(&now));

    fprintf(f, "%d\n", (int)getpid());
    fprintf(f, "%s\n", time_buf);
    fprintf(f, "%s\n", data_dir);
    fprintf(f, "5432\n");  /* port */
    fprintf(f, "*\n");     /* listen_addresses */

    fclose(f);
    return 0;
}

/**
 * @brief 删除 PID 文件
 */
static int remove_pid_file(const char *data_dir) {
    char path[MAX_PATH_LEN];
    get_pid_path(data_dir, path, sizeof(path));
#ifdef _WIN32
    DeleteFileA(path);
#else
    unlink(path);
#endif
    return 0;
}

/**
 * @brief 等待服务器启动
 */
static int wait_for_startup(const char *data_dir, int timeout) {
    printf("等待服务器启动...\n");

    for (int i = 0; i < timeout; i++) {
        if (server_running(data_dir)) {
            printf("服务器已启动 (PID: %d)\n", read_pid_file(data_dir));
            return 0;
        }
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }

    printf("启动超时\n");
    return -1;
}

/**
 * @brief 等待服务器关闭
 */
static int wait_for_shutdown(const char *data_dir, int timeout) {
    printf("等待服务器关闭...\n");

    for (int i = 0; i < timeout; i++) {
        if (!server_running(data_dir)) {
            printf("服务器已关闭\n");
            return 0;
        }
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }

    printf("关闭超时\n");
    return -1;
}

/* ============================================================
 * 命令实现
 * ============================================================ */

/**
 * @brief 启动服务器
 */
static int do_start(const pg_ctl_options_t *opts) {
    const char *data_dir = opts->data_dir;
    char cmd[MAX_PATH_LEN * 2];

    if (!data_dir) {
        LOG_ERROR("数据目录未指定，请使用 -D 选项");
        return -1;
    }

    /* 检查是否已运行 */
    if (server_running(data_dir)) {
        int pid = read_pid_file(data_dir);
        printf("服务器已在运行 (PID: %d)\n", pid);
        return 0;
    }

    /* 检查是否已初始化 */
    if (!initdb_is_initialized(data_dir)) {
        printf("数据目录未初始化，正在初始化...\n");
        initdb_options_t init_opts = {0};
        init_opts.data_dir = data_dir;
        if (initdb(&init_opts) != 0) {
            LOG_ERROR("初始化失败");
            return -1;
        }
    }

    /* 写入 PID 文件 */
    write_pid_file(data_dir);

    /* 启动服务器进程 */
    printf("启动服务器...\n");

#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "start /B db_server.exe -D \"%s\"", data_dir);
#else
    snprintf(cmd, sizeof(cmd), "./db_server -D '%s' &", data_dir);
#endif

    printf("服务器命令: %s\n", cmd);
    printf("(实际启动服务器需要 db_server 可执行文件)\n");

    if (opts->wait) {
        wait_for_startup(data_dir, opts->wait_timeout);
    }

    return 0;
}

/**
 * @brief 停止服务器
 */
static int do_stop(const pg_ctl_options_t *opts) {
    const char *data_dir = opts->data_dir;
    int pid;

    if (!data_dir) {
        LOG_ERROR("数据目录未指定");
        return -1;
    }

    /* 检查是否运行 */
    if (!server_running(data_dir)) {
        printf("服务器未运行\n");
        return 0;
    }

    pid = read_pid_file(data_dir);
    printf("停止服务器 (PID: %d)...\n", pid);

    /* 发送信号 */
#ifdef _WIN32
    if (opts->immediate || opts->force) {
        TerminateProcess(OpenProcess(PROCESS_TERMINATE, FALSE, pid), 0);
    } else {
        /* 优雅关闭：发送 Ctrl+C */
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
    }
#else
    if (opts->immediate || opts->force) {
        kill(pid, SIGKILL);
    } else if (opts->fast) {
        kill(pid, SIGINT);
    } else {
        kill(pid, SIGTERM);
    }
#endif

    if (opts->wait) {
        wait_for_shutdown(data_dir, opts->wait_timeout);
    }

    remove_pid_file(data_dir);
    return 0;
}

/**
 * @brief 重启服务器
 */
static int do_restart(const pg_ctl_options_t *opts) {
    printf("重启服务器...\n");
    do_stop(opts);
    return do_start(opts);
}

/**
 * @brief 重新加载配置
 */
static int do_reload(const pg_ctl_options_t *opts) {
    const char *data_dir = opts->data_dir;
    int pid;

    if (!data_dir) {
        LOG_ERROR("数据目录未指定");
        return -1;
    }

    if (!server_running(data_dir)) {
        printf("服务器未运行\n");
        return -1;
    }

    pid = read_pid_file(data_dir);
    printf("重新加载配置 (PID: %d)...\n", pid);

#ifdef _WIN32
    GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
#else
    kill(pid, SIGHUP);
#endif

    return 0;
}

/**
 * @brief 检查状态
 */
static int do_status(const pg_ctl_options_t *opts) {
    const char *data_dir = opts->data_dir;

    if (!data_dir) {
        LOG_ERROR("数据目录未指定");
        return -1;
    }

    if (!initdb_is_initialized(data_dir)) {
        printf("数据目录未初始化\n");
        return 3;  /* pg_ctl 约定：未初始化返回 3 */
    }

    if (server_running(data_dir)) {
        int pid = read_pid_file(data_dir);
        printf("服务器正在运行 (PID: %d)\n", pid);
        printf("数据目录: %s\n", data_dir);
        return 0;
    } else {
        printf("服务器未运行\n");
        printf("数据目录: %s\n", data_dir);
        return 1;  /* pg_ctl 约定：未运行返回 1 */
    }
}

/**
 * @brief 提升备库为主库
 */
static int do_promote(const pg_ctl_options_t *opts) {
    const char *data_dir = opts->data_dir;

    if (!data_dir) {
        LOG_ERROR("数据目录未指定");
        return -1;
    }

    printf("提升为主库...\n");
    printf("(流复制备库提升功能 stub)\n");

    (void)data_dir;
    return 0;
}

/**
 * @brief 初始化数据库
 */
static int do_initdb(const pg_ctl_options_t *opts) {
    initdb_options_t init_opts = {0};

    if (!opts->data_dir) {
        LOG_ERROR("数据目录未指定");
        return -1;
    }

    init_opts.data_dir = opts->data_dir;
    return initdb(&init_opts);
}

/* ============================================================
 * 公共 API
 * ============================================================ */

int pg_ctl(const pg_ctl_options_t *opts) {
    switch (opts->cmd) {
        case PG_CTL_START:
            return do_start(opts);
        case PG_CTL_STOP:
            return do_stop(opts);
        case PG_CTL_RESTART:
            return do_restart(opts);
        case PG_CTL_RELOAD:
            return do_reload(opts);
        case PG_CTL_STATUS:
            return do_status(opts);
        case PG_CTL_PROMOTE:
            return do_promote(opts);
        case PG_CTL_INITDB:
            return do_initdb(opts);
        default:
            LOG_ERROR("未知命令");
            return -1;
    }
}

int pg_ctl_parse_args(int argc, char *argv[], pg_ctl_options_t *opts) {
    memset(opts, 0, sizeof(*opts));
    opts->cmd = PG_CTL_STATUS;
    opts->wait = true;
    opts->wait_timeout = 60;
    opts->data_dir = getenv("PGDATA");

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "start") == 0) {
            opts->cmd = PG_CTL_START;
        } else if (strcmp(argv[i], "stop") == 0) {
            opts->cmd = PG_CTL_STOP;
        } else if (strcmp(argv[i], "restart") == 0) {
            opts->cmd = PG_CTL_RESTART;
        } else if (strcmp(argv[i], "reload") == 0) {
            opts->cmd = PG_CTL_RELOAD;
        } else if (strcmp(argv[i], "status") == 0) {
            opts->cmd = PG_CTL_STATUS;
        } else if (strcmp(argv[i], "promote") == 0) {
            opts->cmd = PG_CTL_PROMOTE;
        } else if (strcmp(argv[i], "initdb") == 0) {
            opts->cmd = PG_CTL_INITDB;
        } else if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--pgdata") == 0) {
            if (++i < argc) opts->data_dir = argv[i];
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log") == 0) {
            if (++i < argc) opts->log_file = argv[i];
        } else if (strcmp(argv[i], "-o") == 0) {
            if (++i < argc) opts->options = argv[i];
        } else if (strcmp(argv[i], "-w") == 0) {
            opts->wait = true;
        } else if (strcmp(argv[i], "-W") == 0) {
            opts->wait = false;
        } else if (strcmp(argv[i], "-m") == 0) {
            if (++i < argc) {
                if (strcmp(argv[i], "fast") == 0) opts->fast = true;
                else if (strcmp(argv[i], "immediate") == 0) opts->immediate = true;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            pg_ctl_usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--version") == 0) {
            pg_ctl_version();
            exit(0);
        } else if (argv[i][0] != '-') {
            opts->data_dir = argv[i];
        }
    }

    return 0;
}

void pg_ctl_usage(const char *prog) {
    printf("Usage: %s COMMAND [-D DATADIR] [OPTION]...\n\n", prog);
    printf("Commands:\n");
    printf("  initdb     初始化数据库集群\n");
    printf("  start      启动服务器\n");
    printf("  stop       停止服务器\n");
    printf("  restart    重启服务器\n");
    printf("  reload     重新加载配置\n");
    printf("  status     检查服务器状态\n");
    printf("  promote    提升备库为主库\n\n");
    printf("Options:\n");
    printf("  -D DATADIR        数据目录\n");
    printf("  -l FILENAME       日志文件\n");
    printf("  -w                等待完成\n");
    printf("  -W                不等待\n");
    printf("  -m MODE           停止模式 (fast/immediate)\n");
    printf("  --help            显示帮助\n");
    printf("  --version         显示版本\n");
}

void pg_ctl_version(void) {
    printf("pg_ctl (DB) " PG_CTL_VERSION "\n");
}
