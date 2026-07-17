# 数据库初始化（initdb）规格

## Overview

实现 `initdb` 工具，用于初始化数据库集群目录结构，创建系统目录和配置文件。

## initdb 命令

### Usage

```bash
initdb [OPTIONS] <data_directory>

选项:
  -D, --pgdata=PATH       数据目录（等同于最后一个参数）
  -E, --encoding=ENCODING 数据库编码（默认 UTF8）
  -U, --username=NAME     超级用户名称（默认 postgres）
  -W, --pwprompt          提示输入超级用户密码
  --locale=LOCALE         设置区域（默认 C）
  --no-locale             等同于 --locale C
  --auth=METHOD           认证方法（默认 md5）
  --auth-local=METHOD     本地认证方法（默认 peer）
  -V, --version           输出版本并退出
  -?, --help              显示帮助并退出
```

## Directory Structure

```
<data_directory>/
├── postgresql.conf          # 主配置文件
├── pg_hba.conf              # 认证配置文件
├── pg_ident.conf            # 用户映射文件
├── postmaster.pid           # 运行时会创建
├── postmaster.opts          # 启动选项
│
├── base/                    # 数据库表空间
│   └── 1/                   # template1 数据库
│       ├── 1259             # pg_class
│       ├── 1259_fsm         # 空闲空间映射
│       ├── 1259_vm          # 可见性映射
│       └── ...
│
├── global/                  # 共享系统表
│   ├── 1260                 # pg_database
│   └── ...
│
├── pg_wal/                  # WAL 日志目录
│   └── 000000010000000000000001  # 初始 WAL 段
│
├── pg_xact/                 # 事务状态目录
│   └── 0000                # 初始状态文件
│
├── pg_dynshmem/             # 动态共享内存
├── pg_notify/               # LISTEN/NOTIFY 队列
├── pg_serial/               # 序列化提交
├── pg_snapshots/            # 快照导出
├── pg_stat_tmp/             # 统计信息临时文件
├── pg_subtrans/             # 子事务状态
└── pg_tblspc/              # 表空间符号链接
```

## Initialization Steps

```
initdb(data_dir):
    1. 检查目录
       - 如果存在且非空，报错
       - 如果不存在，创建
       - 如果存在但为空，初始化

    2. 创建目录结构
       mkdir -p base global pg_wal pg_xact ...
       chmod 700 data_dir

    3. 创建系统表
       - pg_database: template1, postgres
       - pg_class: 所有系统表定义
       - pg_attribute: 列定义
       - pg_proc: 内置函数
       - pg_type: 数据类型

    4. 创建模板数据库
       - template1: 所有新数据库的模板
       - postgres: 默认数据库

    5. 生成配置文件
       - postgresql.conf (带默认参数)
       - pg_hba.conf (默认认证规则)
       - pg_ident.conf (空配置)

    6. 初始化 WAL
       - 写入初始检查点
       - 创建 pg_wal 子目录

    7. 设置权限
       - data_dir: 0700 (只有 owner)
       - 其他文件: 0600 或 0700

    8. 写入版本标识
       - PG_VERSION 文件
```

## System Tables

### pg_database

| OID | Name | Encoding | ACL |
|-----|------|----------|-----|
| 1 | template1 | UTF8 | {postgres=CTc/postgres} |
| 13388 | postgres | UTF8 | {postgres=CTc/postgres} |

### pg_class (部分)

| OID | Relname | Relkind | Relnamespace |
|-----|---------|---------|--------------|
| 1259 | pg_class | r | 11 (pg_catalog) |
| 1255 | pg_attribute | r | 11 |
| 1247 | pg_type | r | 11 |
| 1260 | pg_database | r | 11 |
| 2615 | pg_tablespace | r | 11 |
| 2612 | pg_authid | r | 11 |

## Configuration Generation

### postgresql.conf 模板

```conf
# PostgreSQL Configuration File

# 连接设置
listen_addresses = 'localhost'
port = 5432
max_connections = 100

# 内存设置
shared_buffers = 128MB
work_mem = 4MB
maintenance_work_mem = 64MB

# WAL 设置
wal_level = replica
fsync = on
synchronous_commit = on

# 查询规划
random_page_cost = 4.0
effective_cache_size = 4GB

# 日志
log_destination = 'stderr'
logging_collector = off
log_connections = off
log_disconnections = off

# 区域
datestyle = 'iso, mdy'
timezone = 'UTC'
lc_messages = 'C'
lc_monetary = 'C'
lc_numeric = 'C'
lc_time = 'C'
default_text_search_config = 'pg_catalog.english'

# 包含额外配置
include_dir = 'conf.d'
```

### pg_hba.conf 默认内容

```conf
# TYPE  DATABASE        USER            ADDRESS                 METHOD

# "local" is for Unix domain socket connections only
local   all             all                                     trust
# IPv4 local connections:
host    all             all             127.0.0.1/32            md5
# IPv6 local connections:
host    all             all             ::1/128                 md5
```

## API

```c
/**
 * @brief initdb 主函数
 * @param data_dir 数据目录
 * @param encoding 编码（NULL 使用默认）
 * @param username 超级用户名
 * @param options 初始化选项
 * @return 0 成功
 */
int initdb(const char *data_dir, const char *encoding,
           const char *username, initdb_options_t *options);

/**
 * @brief 创建数据目录结构
 */
int initdb_create_directories(const char *data_dir);

/**
 * @brief 初始化系统表
 */
int initdb_init_system_tables(const char *data_dir);

/**
 * @brief 生成配置文件
 */
int initdb_write_config_files(const char *data_dir,
                              const char *username);

/**
 * @brief 初始化 WAL
 */
int initdb_init_wal(const char *data_dir);
```

## Acceptance Criteria

- [ ] `initdb /tmp/testdb` 成功创建目录结构
- [ ] `initdb` 不带参数使用 PGDATA 环境变量
- [ ] 已存在非空目录时拒绝初始化
- [ ] 生成的 postgresql.conf 包含所有必需参数
- [ ] 系统表 pg_database 包含 template1 和 postgres
- [ ] pg_hba.conf 默认允许本地 trust 连接
- [ ] 数据目录权限为 0700
- [ ] `pg_ctl start -D /tmp/testdb` 能启动服务器
- [ ] `psql -d postgres` 能连接到数据库
- [ ] `\dt` 能列出系统表
