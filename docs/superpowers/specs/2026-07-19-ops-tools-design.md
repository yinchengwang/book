# 运维工具设计文档：备份恢复 + 在线 DDL

> **版本**: v1.0
> **日期**: 2026-07-19
> **状态**: 已批准

## 1. 目标

为存储引擎提供两个核心运维能力：

1. **备份恢复** — 数据库冷备/恢复，保证数据可迁移、可恢复
2. **在线 DDL** — 运行时修改表结构（ADD/DROP/ALTER/RENAME COLUMN）

## 2. 现状

| 维度 | 状态 |
|------|------|
| **DDL 解析** | 仅有 `TOKEN_ALTER` 词法标记 + `AST_AlterTableStmt` 枚举值，无 `AlterTableStmt` 结构体定义，无 ALTER 解析实现 |
| **DDL 执行** | `execute_ddl()` 走通用 SQL 执行路径，但 ALTER 无对应执行逻辑 |
| **备份恢复** | 无任何备份 API。`rtree_file_repair()` 是唯一带备份参数的工具函数 |
| **文件布局** | 每个 DB 一对文件：`*.db`（数据）+ `*.wal`（WAL 日志） |
| **WAL** | 完整的 Redo/Undo 框架，`wal_redo()` 恢复接口就绪 |

## 3. 架构

### 3.1 备份恢复

#### 备份文件布局

```
<backup_dir>/
├── meta.json               # 备份元数据
├── <db_name>.db            # 数据文件拷贝
└── <db_name>.wal           # WAL 文件拷贝
```

`meta.json` 格式：

```json
{
    "version": 1,
    "db_name": "test_kv",
    "created_at": "2026-07-19T10:30:00Z",
    "db_size": 409600,
    "wal_size": 1024,
    "db_checksum": "sha256hex...",
    "wal_checksum": "sha256hex...",
    "db_version": 1,
    "engine_version": "0.1.0"
}
```

#### 备份流程

```
BACKUP DATABASE '<db_path>' TO '<backup_dir>'
```

1. 打开数据库（只读模式）
2. 执行 `wal_flush()` 确保 WAL 缓冲区落盘
3. 读取 `kv_stats()` 获取统计信息
4. 拷贝 `.db` + `.wal` 到备份目录
5. 计算校验和（SHA-256），写入 `meta.json`
6. 关闭数据库
7. 返回成功/失败

#### 恢复流程

```
RESTORE DATABASE FROM '<backup_dir>' INTO '<db_dir>'
```

1. 读取 `meta.json`，校验元数据完整性
2. 校验 `.db` + `.wal` 校验和
3. 拷贝 `.db` + `.wal` 到目标数据目录
4. 打开数据库 → 自动触发 `kv_replay_wal()` → WAL 重放
5. 验证数据完整性（检查键数量、总大小）
6. 返回成功/失败

#### 备份 API

```c
// backup.h

/** 备份元数据 */
typedef struct backup_meta_s {
    uint32_t version;            /**< 备份格式版本 */
    char     db_name[256];       /**< 数据库名称 */
    char     created_at[64];     /**< 备份时间 ISO8601 */
    uint64_t db_size;            /**< 数据文件大小 */
    uint64_t wal_size;           /**< WAL 文件大小 */
    char     db_checksum[65];    /**< 数据文件 SHA-256（十六进制） */
    char     wal_checksum[65];   /**< WAL 文件 SHA-256（十六进制） */
    uint32_t db_version;         /**< 数据库格式版本 */
    char     engine_version[32]; /**< 引擎版本 */
} backup_meta_t;

/**
 * @brief 备份数据库
 * @param db_path   数据库路径（*.db 文件）
 * @param backup_dir 备份目标目录
 * @return 0 成功，-1 失败
 */
int db_backup(const char *db_path, const char *backup_dir);

/**
 * @brief 恢复数据库
 * @param backup_dir 备份目录
 * @param db_dir     目标数据目录
 * @return 0 成功，-1 失败
 */
int db_restore(const char *backup_dir, const char *db_dir);

/**
 * @brief 获取备份元数据
 * @param backup_dir 备份目录
 * @param meta       输出元数据
 * @return 0 成功，-1 失败
 */
int db_backup_meta(const char *backup_dir, backup_meta_t *meta);
```

#### CLI 工具

**db_backup** — 备份命令行工具：

```
db_backup <db_path> <backup_dir>
```

示例：
```bash
# 备份 test_kv.db 到 /backup/20260719/
db_backup ./data/test_kv.db /backup/20260719/

# 成功输出
Backup completed: /backup/20260719/
  db_size: 409600 bytes
  wal_size: 1024 bytes
  checksum: a1b2c3...
```

**db_restore** — 恢复命令行工具：

```
db_restore <backup_dir> <db_dir>
```

示例：
```bash
# 从 /backup/20260719/ 恢复到 ./data/
db_restore /backup/20260719/ ./data/

# 成功输出
Restore completed: ./data/test_kv.db
  keys: 42
  total_size: 409600 bytes
```

### 3.2 在线 DDL

#### ALTER TABLE 语法

SQL 解析器扩展支持以下语法：

```sql
-- ADD COLUMN
ALTER TABLE users ADD COLUMN email VARCHAR(255);
ALTER TABLE users ADD age INT NOT NULL DEFAULT 0;

-- DROP COLUMN
ALTER TABLE users DROP COLUMN email;

-- ALTER COLUMN TYPE
ALTER TABLE users ALTER COLUMN age TYPE BIGINT;
ALTER TABLE users ALTER COLUMN name SET DATA TYPE VARCHAR(100);

-- RENAME COLUMN
ALTER TABLE users RENAME COLUMN email TO email_addr;
```

#### AST 节点

```c
/** ALTER TABLE 操作类型 */
typedef enum AlterTableOp_e {
    AT_AddColumn,          /**< ADD COLUMN */
    AT_DropColumn,         /**< DROP COLUMN */
    AT_AlterColumnType,    /**< ALTER COLUMN TYPE */
    AT_RenameColumn        /**< RENAME COLUMN */
} AlterTableOp;

/** ALTER TABLE 语句 */
typedef struct AlterTableStmt_s {
    SqlAstType type;
    char *relation;                /**< 表名 */
    AlterTableOp operation;        /**< 操作类型 */
    int num_cmds;                  /**< 命令数量 */
    void **cmds;                   /**< 命令数组（AlterTableCmd*） */
    int location;
} AlterTableStmt;

/** ALTER TABLE 子命令 */
typedef struct AlterTableCmd_s {
    SqlAstType type;
    AlterTableOp subtype;          /**< 子类型 */
    char *name;                    /**< 列名 */
    char *new_name;                /**< 新列名（RENAME） */
    char *type_name;               /**< 新类型名（ALTER TYPE） */
    char *default_expr;            /**< 默认值表达式（ADD） */
    bool not_null;                 /**< NOT NULL（ADD） */
    int location;
} AlterTableCmd;
```

#### 执行策略

| 操作 | 策略 | 说明 |
|------|------|------|
| **ADD COLUMN** | 仅元数据 | 更新 Catalog 列定义，旧页面读取时补默认值 |
| **DROP COLUMN** | 仅元数据 | 更新 Catalog（标记删除），数据不立即回收 |
| **ALTER COLUMN TYPE** | 逐行迁移 | 读取旧值 → 类型转换 → 写入新值 |
| **RENAME COLUMN** | 仅元数据 | 更新 Catalog 列名 |

**ADD COLUMN / DROP COLUMN** 为"仅元数据"操作，**不需要重写数据文件**。`TupleDesc` 中新增列版本号，读取页面时根据版本补默认值或跳过已删除列。

**ALTER COLUMN TYPE** 需要逐行转换，流程：

1. 锁表（防止并发写入）
2. 创建新列（临时）
3. 逐行扫描：读取旧值 → 调用类型转换函数 → 写入新列
4. 提交事务，更新 Catalog
5. 解锁

#### 执行器实现

```c
// sql_ddl.h

/**
 * @brief 执行 ALTER TABLE 语句
 * @param stmt ALTER TABLE 语句 AST
 * @param db   数据库句柄
 * @return 0 成功，-1 失败
 */
int exec_alter_table(AlterTableStmt *stmt, void *db);
```

```c
// exec_alter_table 实现概要
int exec_alter_table(AlterTableStmt *stmt, void *db) {
    // 1. 获取表元数据
    table_t *table = table_open(db, stmt->relation);
    if (!table) return -1;

    // 2. 遍历命令列表
    for (int i = 0; i < stmt->num_cmds; i++) {
        AlterTableCmd *cmd = stmt->cmds[i];
        switch (cmd->subtype) {
            case AT_AddColumn:
                // 更新 Catalog 列定义
                catalog_add_column(table, cmd->name, cmd->type_name, cmd->not_null);
                break;
            case AT_DropColumn:
                // 标记删除列
                catalog_drop_column(table, cmd->name);
                break;
            case AT_AlterColumnType:
                // 逐行类型转换
                alter_column_type(table, cmd->name, cmd->type_name);
                break;
            case AT_RenameColumn:
                // 重命名列
                catalog_rename_column(table, cmd->name, cmd->new_name);
                break;
        }
    }

    table_close(table);
    return 0;
}
```

## 4. 变更文件清单

### 新增文件

| 文件 | 说明 |
|------|------|
| `engineering/include/db/backup.h` | 备份恢复接口 |
| `engineering/src/db/core/backup.c` | 备份恢复实现 |
| `engineering/include/db/sql/sql_ddl.h` | DDL 执行器头文件 |
| `engineering/src/db/sql/sql_ddl.c` | DDL 执行器实现 |
| `engineering/apps/tools/db_backup/main.c` | 备份 CLI 工具 |
| `engineering/apps/tools/db_restore/main.c` | 恢复 CLI 工具 |
| `engineering/test/db/core/backup_test.cpp` | 备份恢复测试 |
| `engineering/test/db/sql/ddl_test.cpp` | DDL 执行测试 |

### 修改文件

| 文件 | 变更说明 |
|------|---------|
| `engineering/include/db/sql/sql_parser.h` | 新增 `AlterTableStmt`/`AlterTableCmd` 结构体、`AlterTableOp` 枚举 |
| `engineering/src/db/sql/sql_parser.c` | 实现 ALTER TABLE 语法解析 |
| `engineering/src/db/cli/cli.c` | 新增 BACKUP/RESTORE SQL 命令处理 |
| `engineering/test/db/storage/CMakeLists.txt` | 添加备份恢复测试目标 |
| `engineering/test/db/sql/CMakeLists.txt` | 添加 DDL 测试目标 |
| `engineering/CMakeLists.txt` | 添加 db_backup/db_restore 子目录 |

## 5. 测试策略

### 备份恢复测试

| 测试名 | 说明 |
|--------|------|
| `BackupRestoreBasic` | 写入数据 → 备份 → 删除原库 → 恢复 → 验证数据一致 |
| `BackupEmptyDB` | 空数据库备份恢复 |
| `BackupCorruptedFile` | 备份文件损坏时恢复失败 |
| `RestoreCheckIntegrity` | 恢复后自动触发 WAL 重放，数据完整 |
| `BackupOverwrite` | 备份到已存在目录的处理 |

### DDL 测试

| 测试名 | 说明 |
|--------|------|
| `AlterTableAddColumn` | ADD COLUMN 后查询正常 |
| `AlterTableDropColumn` | DROP COLUMN 后查询不含被删列 |
| `AlterTableAlterType` | ALTER COLUMN TYPE 后类型转换正确 |
| `AlterTableRenameColumn` | RENAME COLUMN 后用新列名可查询 |
| `AlterTableBatch` | 多命令批量执行 |
| `AlterTableNonExistentTable` | 不存在的表返回错误 |
| `AlterTableInvalidColumn` | 不存在的列返回错误 |

## 6. 阶段划分

### Phase 1：备份恢复（基础版）

1. 实现 `backup.h` / `backup.c`（文件级拷贝 + 校验和 + meta.json）
2. 实现 `db_backup` CLI 工具
3. 实现 `db_restore` CLI 工具
4. 备份恢复测试

### Phase 2：在线 DDL

1. 扩展 `sql_parser.h` — 新增 `AlterTableStmt` 结构体 + `AlterTableOp` 枚举
2. 实现 ALTER TABLE 语法解析（`sql_parser.c`）
3. 实现 DDL 执行器（`sql_ddl.c`）
4. DDL 测试

## 7. 验收标准

- [ ] `db_backup` 正确拷贝 .db + .wal 文件，生成 meta.json
- [ ] `db_restore` 正确恢复数据，自动重放 WAL
- [ ] 备份文件损坏时恢复失败并报错
- [ ] `ALTER TABLE ADD COLUMN` 执行后表结构包含新列
- [ ] `ALTER TABLE DROP COLUMN` 执行后查询不含被删列
- [ ] `ALTER TABLE ALTER COLUMN TYPE` 执行后类型正确转换
- [ ] `ALTER TABLE RENAME COLUMN` 执行后用新列名可查询
- [ ] 不存在的表/列返回明确错误
- [ ] 所有测试通过