## Why

当前数据库 SQL 层存在多个关键功能缺失：
1. 词法分析器缺少浮点数支持，解析器 WHERE 子句功能不完整
2. SELECT/UPDATE/DELETE 核心语句未实现执行逻辑
3. 缺少 PostgreSQL 风格的 GUC 配置系统
4. 缺少数据库集群初始化（initdb）和服务器启动机制

这些问题严重限制了数据库的实用性和对 PostgreSQL 的兼容性，必须系统性地解决。

## What Changes

### SQL 层增强
- **修复浮点数解析**: 添加 `SQL_TOKEN_FLOAT` 支持，词法分析器识别小数点格式数字
- **增强 WHERE 表达式**: 支持 AND/OR/NOT/LIKE/IN/BETWEEN/IS NULL 等操作符
- **实现 SELECT 执行**: 完整的表扫描、过滤、投影逻辑
- **实现 UPDATE 执行**: 扫描匹配行、计算新值、更新元组
- **实现 DELETE 执行**: 扫描匹配行、标记删除

### 配置系统
- **新增 GUC 系统**: 实现 `db_config.h/c`，支持运行时参数获取/设置
- **配置文件解析**: 支持 `postgresql.conf` 格式的配置文件加载
- **核心配置项**: shared_buffers、wal_level、checkpoint_timeout、work_mem 等

### 启动机制
- **initdb 工具**: 初始化数据库集群目录结构
- **服务器启动**: 支持后台服务模式和客户端连接监听
- **客户端协议**: 简化的 PostgreSQL wire 协议支持

### Bug 修复
- **修复死锁检测竞态**: 使用 `std::call_once` 确保静态初始化线程安全

## Capabilities

### New Capabilities

- **sql-float-support**: 词法分析器浮点数识别和解析器处理
- **sql-where-expressions**: 完整的 WHERE 条件表达式解析
- **sql-select-execution**: SELECT 语句执行引擎
- **sql-update-execution**: UPDATE 语句执行引擎
- **sql-delete-execution**: DELETE 语句执行引擎
- **guc-config-system**: PostgreSQL 风格的 GUC 配置系统
- **db-initdb**: 数据库集群初始化工具
- **db-server**: 数据库服务器启动和连接管理

### Modified Capabilities

- 无现有规格修改

## Impact

### 受影响代码

| 路径 | 影响 |
|------|------|
| `src/db/sql/sql_lexer.c` | 添加浮点数 token 识别 |
| `src/db/sql/sql_parser.c` | 增强表达式解析，实现 SELECT/UPDATE/DELETE |
| `src/db/sql/sql_exec.c` | 实现核心执行逻辑 |
| `include/db/` | 新增 config.h 头文件 |
| `src/db/` | 新增 config.c 配置管理 |
| `src/db/cli/` | 新增 initdb 命令和服务器模式 |

### 依赖关系

```
sql-select-execution ──→ sql-where-expressions ──→ sql-float-support
sql-update-execution  ──→ sql-where-expressions
sql-delete-execution  ──→ sql-where-expressions
db-server             ──→ guc-config-system
db-initdb             ──→ guc-config-system
```

### 向后兼容性

所有新增功能均为增量添加，不影响现有 API 契约。
