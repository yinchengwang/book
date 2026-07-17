# GUC 配置系统规格

## Overview

实现 PostgreSQL 风格的 GUC（Grand Unified Configuration）系统，支持配置文件和运行时参数管理。

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    GUC Manager (单例)                       │
├─────────────────────────────────────────────────────────────┤
│  参数表 (guc_param_t[])                                     │
│  ├── shared_buffers      │ 100MB    │ 动态                  │
│  ├── wal_level           │ replica  │ 需重启                │
│  ├── checkpoint_timeout  │ 5min     │ 需重启                │
│  └── ...                                                   │
├─────────────────────────────────────────────────────────────┤
│  配置源                                                      │
│  ├── 编译时默认值                                            │
│  ├── postgresql.conf 文件                                    │
│  └── 运行时 SET 命令                                         │
└─────────────────────────────────────────────────────────────┘
```

## Core Types

### guc_type_t

```c
typedef enum guc_type_e {
    GUC_TYPE_BOOL,      /**< 布尔值 */
    GUC_TYPE_INT,       /**< 整数 */
    GUC_TYPE_REAL,      /**< 浮点数 */
    GUC_TYPE_STRING,    /**< 字符串 */
    GUC_TYPE_ENUM       /**< 枚举值 */
} guc_type_t;
```

### guc_param_t

```c
typedef struct guc_param_s {
    const char      *name;           /**< 参数名 */
    guc_type_t       type;           /**< 参数类型 */
    void            *value;          /**< 当前值 */
    void            *reset_val;      /**< 重置值（ALTER ... SET DEFAULT） */
    union {
        struct {
            int     min;             /**< 最小值 */
            int     max;             /**< 最大值 */
        } int_v;
        struct {
            double  min;             /**< 最小值 */
            double  max;             /**< 最大值 */
        } real_v;
        struct {
            const char **options;    /**< 枚举选项列表 */
            int         noptions;    /**< 选项数量 */
        } enum_v;
    } bounds;
    const char      *description;    /**< 参数描述 */
    int              flags;          /**< GUC 标志 */
} guc_param_t;
```

### GUC Flags

```c
#define GUC_EXPLAIN      0x0001  /**< 可在 EXPLAIN 中显示 */
#define GUC_NO_SHOW_ALL  0x0002  /**< 不在 SHOW ALL 中显示 */
#define GUC_NOT_WATCH    0x0004  /**< 不在 watch 列表中 */
#define GUC_NO_RESET_ALL 0x0008  /**< ALTER ... RESET ALL 不影响此参数 */
#define GUC_RELOAD       0x0010  /**< 配置文件重新加载时生效 */
#define GUC_RUNTIME_COMPUTED 0x0020  /**< 运行时计算，不可 SET */
```

## API

### 生命周期

```c
/**
 * @brief 初始化 GUC 系统
 * @param db_path 数据库路径（用于查找配置文件）
 * @return 0 成功
 */
int guc_init(const char *db_path);

/**
 * @brief 销毁 GUC 系统
 */
void guc_shutdown(void);
```

### 参数操作

```c
/**
 * @brief 设置参数值
 * @param name 参数名
 * @param value 值字符串
 * @return 0 成功，-1 失败
 */
int guc_set(const char *name, const char *value);

/**
 * @brief 获取参数值
 * @param name 参数名
 * @param buf 输出缓冲区
 * @param buflen 缓冲区长度
 * @return 0 成功，-1 失败
 */
int guc_get(const char *name, char *buf, size_t buflen);

/**
 * @brief 重置参数为默认值
 * @param name 参数名（NULL 表示全部）
 */
void guc_reset(const char *name);

/**
 * @brief 获取参数信息
 * @param name 参数名
 * @return 参数信息，NULL 不存在
 */
const guc_param_t *guc_get_param(const char *name);
```

### 配置文件

```c
/**
 * @brief 加载配置文件
 * @param path 配置文件路径（NULL 使用默认路径）
 * @return 0 成功，-1 失败
 */
int guc_load_file(const char *path);

/**
 * @brief 保存当前配置到文件
 * @param path 输出路径
 * @return 0 成功
 */
int guc_save_file(const char *path);
```

### 枚举值转换

```c
/**
 * @brief 设置枚举参数
 * @param param 参数
 * @param value 值字符串
 * @return 0 成功
 */
int guc_set_enum(guc_param_t *param, const char *value);

/**
 * @brief 获取枚举值对应的整数
 * @param param 参数
 * @return 枚举值索引
 */
int guc_get_enum(const guc_param_t *param);
```

## Core Parameters

### 内存相关

| 参数名 | 类型 | 默认值 | 范围 | 说明 |
|--------|------|--------|------|------|
| shared_buffers | INT | 16384 | 16-262144 | Buffer 数量（8KB 每块） |
| work_mem | INT | 4096 | 64-1048576 | 排序/哈希内存（KB） |
| maintenance_work_mem | INT | 65536 | 1024-1048576 | 维护操作内存（KB） |
| temp_buffers | INT | 1024 | 100-1048576 | 临时 Buffer（KB） |

### WAL 相关

| 参数名 | 类型 | 默认值 | 选项 | 说明 |
|--------|------|--------|------|------|
| wal_level | ENUM | replica | minimal/replica/logical | WAL 级别 |
| fsync | BOOL | on | - | 是否同步刷盘 |
| synchronous_commit | ENUM | on | on/off/local | 同步提交模式 |
| wal_buffers | INT | -1 | -1/8-4096 | WAL Buffer 数量 |

### 查询规划

| 参数名 | 类型 | 默认值 | 范围 | 说明 |
|--------|------|--------|------|------|
| random_page_cost | REAL | 4.0 | 0-1e6 | 随机页访问成本 |
| seq_page_cost | REAL | 1.0 | 0-1e6 | 顺序页访问成本 |
| effective_cache_size | INT | 524288 | 1-INT_MAX | 有效缓存大小（8KB 页） |

### 检查点

| 参数名 | 类型 | 默认值 | 范围 | 说明 |
|--------|------|--------|------|------|
| checkpoint_timeout | INT | 300 | 30-3600 | 检查点间隔（秒） |
| checkpoint_completion_target | REAL | 0.5 | 0-1 | 检查点完成目标 |
| max_wal_size | INT | 1024 | 8-16384 | 最大 WAL 大小（MB） |

### 客户端连接

| 参数名 | 类型 | 默认值 | 范围 | 说明 |
|--------|------|--------|------|------|
| max_connections | INT | 100 | 1-262143 | 最大连接数 |
| listen_addresses | STRING | localhost | - | 监听地址 |
| port | INT | 5432 | 1-65535 | 监听端口 |

## Config File Format

支持 postgresql.conf 格式：

```conf
# 这是一条注释
shared_buffers = 128MB          # 可带单位
work_mem = 64MB

# 使用单引号或双引号
search_path = '"$user", public'

# 使用 GUC 变量
data_directory = '/var/lib/postgresql/data'

# 条件配置（简化支持）
# if (os == 'Linux') {
#   shared_buffers = 256MB
# }

# include 指令
# include 'extra.conf'
```

### 单位支持

| 单位 | 含义 | 示例 |
|------|------|------|
| kB | 千字节 | 1024 |
| MB | 兆字节 | 1024*1024 |
| GB | 吉字节 | 1024*1024*1024 |
| kB | 千字节（8KB 块） | 128 |
| ms | 毫秒 | 500 |
| s | 秒 | 30 |
| min | 分钟 | 5 |
| h | 小时 | 1 |

## Acceptance Criteria

- [ ] `guc_init()` 正确初始化所有参数
- [ ] `guc_set("shared_buffers", "256MB")` 设置成功
- [ ] `guc_get("shared_buffers", buf, len)` 返回 "256MB"
- [ ] `guc_load_file("postgresql.conf")` 解析成功
- [ ] 无效参数名返回错误
- [ ] 类型不匹配返回错误（如给 INT 设置字符串）
- [ ] `guc_reset(NULL)` 重置所有参数
- [ ] `SHOW ALL` 输出所有参数
- [ ] `SET work_mem = '64MB'` 运行时修改
- [ ] `ALTER ... SET DEFAULT` 恢复默认值
