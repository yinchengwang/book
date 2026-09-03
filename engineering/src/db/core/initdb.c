/**
 * @file initdb.c
 * @brief initdb 工具实现
 */

#include "db/initdb.h"
#include "db/errors.h"
#include "db/log.h"
#include "db/guc.h"
#include "db/catalog.h"
#include "db/wal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

#define INITDB_VERSION "1.0.0"
#define MAX_PATH_LEN 1024

/** 默认目录 */
static const char *SUBDIRS[] = {
    "base",
    "global",
    "pg_wal",
    "pg_xact",
    "pg_commit_ts",
    "pg_dynshmem",
    "pg_notify",
    "pg_replslot",
    "pg_serial",
    "pg_snapshots",
    "pg_stat",
    "pg_stat_tmp",
    "pg_subtrans",
    "pg_tblspc",
    "pg_twophase",
    "pg_walarchive",
    NULL
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 确保目录存在
 */
static int ensure_dir(const char *path) {
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL) == 0) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            LOG_ERROR("无法创建目录: %s (错误码: %lu)", path, err);
            return -1;
        }
    }
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno != ENOENT) {
            LOG_ERROR("无法检查目录: %s", path);
            return -1;
        }
        if (mkdir(path, 0755) != 0) {
            LOG_ERROR("无法创建目录: %s (%s)", path, strerror(errno));
            return -1;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        LOG_ERROR("路径不是目录: %s", path);
        return -1;
    }
#endif
    return 0;
}

/**
 * @brief 创建目录树
 */
static int create_dirs(const char *base) {
    char path[MAX_PATH_LEN];

    for (int i = 0; SUBDIRS[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%s/%s", base, SUBDIRS[i]);
        if (ensure_dir(path) != 0) {
            return -1;
        }
        LOG_INFO("创建目录: %s", path);
    }
    return 0;
}

/**
 * @brief 写入版本文件
 */
static int write_version_file(const char *data_dir) {
    char path[MAX_PATH_LEN];
    FILE *f;

    snprintf(path, sizeof(path), "%s/PG_VERSION", data_dir);
    f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("无法写入 PG_VERSION: %s", strerror(errno));
        return -1;
    }
    fprintf(f, "16\n");  /* 模拟 PostgreSQL 16 */
    fclose(f);

    LOG_INFO("写入版本文件: %s", path);
    return 0;
}

/* ============================================================
 * 系统表创建
 * ============================================================ */

/**
 * @brief 创建 pg_database 系统表
 */
static int create_pg_database(const char *data_dir) {
    char path[MAX_PATH_LEN];
    FILE *f;

    snprintf(path, sizeof(path), "%s/global/pg_database.dat", data_dir);
    f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR("无法创建 pg_database: %s", strerror(errno));
        return -1;
    }

    /* 写入 pg_database 表结构 */
    fprintf(f, "# pg_database 系统表\n");
    fprintf(f, "# 列: oid, datname, datdba, encoding, datcollate, datctype, datistemplate, datallowconn, datconnlimit, datfrozenxid, datminmxid, dattablespace, datacl\n");
    fprintf(f, "1|postgres|1|6|C|C|false|true|-1|0|1|1663||\n");
    fprintf(f, "12678|template0|1|6|C|C|true|false|-1|0|1|1663||\n");
    fprintf(f, "12679|template1|1|6|C|C|true|true|-1|0|1|1663||\n");

    fclose(f);
    LOG_INFO("创建 pg_database 系统表");
    return 0;
}

/**
 * @brief 创建 pg_class 系统表
 */
static int create_pg_class(const char *data_dir) {
    char path[MAX_PATH_LEN];
    FILE *f;

    snprintf(path, sizeof(path), "%s/global/pg_class.dat", data_dir);
    f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR("无法创建 pg_class: %s", strerror(errno));
        return -1;
    }

    fprintf(f, "# pg_class 系统表\n");
    fprintf(f, "# oid, relname, relnamespace, reltype, relowner, relam, relfilenode, reltablespace, relpages, reltuples, relallvisible, reltoastrelid, relhasindex, relisshared, relpersistence, relkind, relnatts, relnumns, relchecks, relhasoids, relhasrules, relhastriggers, relhassubclass, relhumantypeid, relpartbound, relispartition, relrewrite, relfrozenxid, relminxid, relacl, reloptions, relpartbounddatums\n");

    fclose(f);
    LOG_INFO("创建 pg_class 系统表");
    return 0;
}

/**
 * @brief 创建 pg_authid 系统表
 */
static int create_pg_authid(const char *data_dir) {
    char path[MAX_PATH_LEN];
    FILE *f;

    snprintf(path, sizeof(path), "%s/global/pg_authid.dat", data_dir);
    f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR("无法创建 pg_authid: %s", strerror(errno));
        return -1;
    }

    fprintf(f, "# pg_authid 系统表\n");
    fprintf(f, "# 列: oid, rolname, rolsuper, rolinherit, rolcreaterole, rolcreatedb, rolcanlogin, rolreplication, rolbypassrls, rolconnlimit, rolpassword, rolvaliduntil\n");
    fprintf(f, "1|pg_database_admin|true|true|true|true|true|false|false|-1|||t\n");
    fprintf(f, "10|postgres|true|true|true|true|true|true|true|-1|||\n");

    fclose(f);
    LOG_INFO("创建 pg_authid 系统表");
    return 0;
}

/**
 * @brief 创建所有系统表
 */
static int create_system_tables(const char *data_dir) {
    if (create_pg_database(data_dir) != 0) return -1;
    if (create_pg_class(data_dir) != 0) return -1;
    if (create_pg_authid(data_dir) != 0) return -1;
    return 0;
}

/* ============================================================
 * 配置文件生成
 * ============================================================ */

/**
 * @brief 生成 postgresql.conf
 */
static int generate_postgresql_conf(const char *data_dir) {
    char path[MAX_PATH_LEN];
    FILE *f;

    snprintf(path, sizeof(path), "%s/postgresql.conf", data_dir);
    f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("无法创建 postgresql.conf: %s", strerror(errno));
        return -1;
    }

    fprintf(f, "# PostgreSQL Configuration File\n");
    fprintf(f, "# 由 initdb 生成\n\n");

    fprintf(f, "# 连接设置\n");
    fprintf(f, "listen_addresses = 'localhost'\n");
    fprintf(f, "port = 5432\n");
    fprintf(f, "max_connections = 100\n\n");

    fprintf(f, "# 内存设置\n");
    fprintf(f, "shared_buffers = 128MB\n");
    fprintf(f, "work_mem = 4MB\n");
    fprintf(f, "maintenance_work_mem = 64MB\n\n");

    fprintf(f, "# WAL 设置\n");
    fprintf(f, "wal_level = replica\n");
    fprintf(f, "fsync = on\n");
    fprintf(f, "synchronous_commit = on\n");
    fprintf(f, "max_wal_size = 1GB\n\n");

    fprintf(f, "# 日志设置\n");
    fprintf(f, "logging_collector = off\n");
    fprintf(f, "log_destination = 'stderr'\n\n");

    fprintf(f, "# 客户端默认设置\n");
    fprintf(f, "client_min_messages = notice\n");
    fprintf(f, "search_path = '\"$user\", public'\n");
    fprintf(f, "datestyle = 'iso, mdy'\n");
    fprintf(f, "timezone = 'UTC'\n");
    fprintf(f, "lc_messages = 'C'\n");
    fprintf(f, "lc_monetary = 'C'\n");
    fprintf(f, "lc_numeric = 'C'\n");
    fprintf(f, "lc_time = 'C'\n");
    fprintf(f, "default_text_search_config = 'pg_catalog.simple'\n\n");

    fclose(f);
    LOG_INFO("生成 postgresql.conf");
    return 0;
}

/**
 * @brief 生成 pg_hba.conf
 */
static int generate_pg_hba_conf(const char *data_dir) {
    char path[MAX_PATH_LEN];
    FILE *f;

    snprintf(path, sizeof(path), "%s/pg_hba.conf", data_dir);
    f = fopen(path, "w");
    if (!f) {
        LOG_ERROR("无法创建 pg_hba.conf: %s", strerror(errno));
        return -1;
    }

    fprintf(f, "# PostgreSQL Client Authentication Configuration File\n");
    fprintf(f, "# 由 initdb 生成\n\n");

    fprintf(f, "# TYPE  DATABASE        USER            ADDRESS                 METHOD\n\n");

    fprintf(f, "# 本地连接\n");
    fprintf(f, "local   all             all                                     trust\n\n");

    fprintf(f, "# IPv4 本地连接\n");
    fprintf(f, "host    all             all             127.0.0.1/32            trust\n\n");

    fprintf(f, "# IPv6 本地连接\n");
    fprintf(f, "host    all             all             ::1/128                 trust\n\n");

    fprintf(f, "# 允许复制连接\n");
    fprintf(f, "local   replication     all                                     trust\n");
    fprintf(f, "host    replication     all             127.0.0.1/32            trust\n");
    fprintf(f, "host    replication     all             ::1/128                 trust\n");

    fclose(f);
    LOG_INFO("生成 pg_hba.conf");
    return 0;
}

/* ============================================================
 * WAL 初始化
 * ============================================================ */

/**
 * @brief 初始化 WAL
 */
static int init_wal(const char *data_dir) {
    char path[MAX_PATH_LEN];
    FILE *f;

    /* 创建 WAL segment 文件 */
    snprintf(path, sizeof(path), "%s/pg_wal/000000010000000000000001", data_dir);
    f = fopen(path, "wb");
    if (!f) {
        LOG_ERROR("无法创建 WAL segment: %s", strerror(errno));
        return -1;
    }

    /* 16MB WAL segment */
    fseek(f, 16 * 1024 * 1024 - 1, SEEK_SET);
    fputc(0, f);
    fclose(f);

    LOG_INFO("初始化 WAL");
    return 0;
}

/* ============================================================
 * 公共 API
 * ============================================================ */

int initdb(const initdb_options_t *opts) {
    const char *data_dir;
    time_t start_time, end_time;

    if (!opts || !opts->data_dir) {
        LOG_ERROR("数据目录未指定");
        return -1;
    }

    data_dir = opts->data_dir;
    start_time = time(NULL);

    LOG_INFO("=== initdb 开始 ===");
    LOG_INFO("数据目录: %s", data_dir);

    /* 检查是否已初始化 */
    if (initdb_is_initialized(data_dir)) {
        LOG_ERROR("数据目录已初始化，请使用 pg_ctl 启动");
        return -1;
    }

    /* 创建目录结构 */
    LOG_INFO("创建目录结构...");
    if (create_dirs(data_dir) != 0) return -1;

    /* 写入版本文件 */
    LOG_INFO("写入版本文件...");
    if (write_version_file(data_dir) != 0) return -1;

    /* 创建系统表 */
    LOG_INFO("创建系统表...");
    if (create_system_tables(data_dir) != 0) return -1;

    /* 生成配置文件 */
    LOG_INFO("生成配置文件...");
    if (generate_postgresql_conf(data_dir) != 0) return -1;
    if (generate_pg_hba_conf(data_dir) != 0) return -1;

    /* 初始化 WAL */
    LOG_INFO("初始化 WAL...");
    if (init_wal(data_dir) != 0) return -1;

    end_time = time(NULL);
    LOG_INFO("=== initdb 完成，用时 %ld 秒 ===", (long)(end_time - start_time));

    return 0;
}

initdb_options_t *initdb_default_options(void) {
    static initdb_options_t default_opts = {
        .data_dir = "./data",
        .username = "postgres",
        .encoding = "UTF8",
        .locale = "C",
        .no_locale = false,
        .auth_local = true,
        .auth_method = "trust",
        .pw_filename = false,
        .skip_checksums = false,
        .dry_run = false
    };
    return &default_opts;
}

int initdb_parse_args(int argc, char *argv[], initdb_options_t *opts) {
    memset(opts, 0, sizeof(*opts));
    opts->data_dir = "./data";
    opts->username = "postgres";
    opts->encoding = "UTF8";
    opts->locale = "C";
    opts->auth_method = "trust";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-D") == 0 || strcmp(argv[i], "--pgdata") == 0) {
            if (++i < argc) opts->data_dir = argv[i];
        } else if (strcmp(argv[i], "-U") == 0 || strcmp(argv[i], "--username") == 0) {
            if (++i < argc) opts->username = argv[i];
        } else if (strcmp(argv[i], "-E") == 0 || strcmp(argv[i], "--encoding") == 0) {
            if (++i < argc) opts->encoding = argv[i];
        } else if (strcmp(argv[i], "--no-locale") == 0) {
            opts->no_locale = true;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--dry-run") == 0) {
            opts->dry_run = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            initdb_usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("initdb (DB) " INITDB_VERSION "\n");
            exit(0);
        } else if (argv[i][0] != '-') {
            opts->data_dir = argv[i];
        }
    }

    return 0;
}

void initdb_usage(const char *prog) {
    printf("Usage: %s [OPTION]... [DATADIR]\n\n", prog);
    printf("initdb 初始化一个 PostgreSQL 数据库集群。\n\n");
    printf("Options:\n");
    printf("  -D, --pgdata=DIR        数据目录位置\n");
    printf("  -U, --username=NAME     数据库超级用户名\n");
    printf("  -E, --encoding=ENCODING 数据库编码\n");
    printf("  --no-locale             不使用区域设置\n");
    printf("  -n, --dry-run           干运行，不创建任何文件\n");
    printf("  -h, --help              显示帮助\n");
    printf("  -V, --version           显示版本\n");
}

bool initdb_is_initialized(const char *data_dir) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/PG_VERSION", data_dir);

    FILE *f = fopen(path, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

int initdb_create_directories(const char *data_dir) {
    return create_dirs(data_dir);
}

int initdb_create_system_tables(const char *data_dir) {
    return create_system_tables(data_dir);
}

int initdb_create_template_db(const char *data_dir) {
    (void)data_dir;
    LOG_INFO("创建模板数据库 (stub)");
    return 0;
}

int initdb_generate_config(const char *data_dir) {
    return generate_postgresql_conf(data_dir);
}

int initdb_generate_hba(const char *data_dir) {
    return generate_pg_hba_conf(data_dir);
}

int initdb_init_wal(const char *data_dir) {
    return init_wal(data_dir);
}
