## Context

当前数据库实现了一个基础的 PostgreSQL 风格存储架构（Catalog、Buffer Pool、Heap/BTree AM、WAL），但在 SQL 执行层存在显著的功能缺失：

1. **SQL 解析层**: 词法分析器无法识别浮点数，WHERE 子句只支持简单比较
2. **SQL 执行层**: SELECT/UPDATE/DELETE 只有桩实现，返回 "Not implemented"
3. **配置系统**: 没有类似 PostgreSQL GUC 的运行时配置机制
4. **启动机制**: 缺少 initdb 初始化和服务器模式

这些缺失严重影响了数据库的可用性和与 PostgreSQL 的兼容性。

## Goals / Non-Goals

**Goals:**
- 实现完整的 SQL 表达式解析，支持 AND/OR/NOT/LIKE/IN/BETWEEN/IS NULL
- 实现 SELECT/UPDATE/DELETE 语句的执行逻辑
- 建立 GUC 风格的配置系统，支持 postgresql.conf 格式
- 提供 initdb 工具初始化数据库集群
- 支持基本的服务器模式（监听连接）

**Non-Goals:**
- 不实现完整的 PostgreSQL wire 协议（简化版即可）
- 不实现视图、触发器、存储过程
- 不实现完整的 MVCC 隔离级别（保持当前的脏读）
- 不实现两阶段提交

## Decisions

### Decision 1: SQL 表达式解析架构

**选择**: 递归下降解析器 + 操作符优先级表

**理由**:
- 现有代码已使用递归下降解析器，风格一致
- 操作符优先级表可以处理复杂的表达式嵌套
- 比 Yacc/Bison 生成的 LALR 解析器更轻量，易于维护

**替代方案考虑**:
- Yacc/Bison: 功能更强但依赖外部工具，增加构建复杂度
- 手写状态机: 适合简单语法，但复杂表达式难以处理

### Decision 2: 配置系统设计

**选择**: 单例模式 GUC 参数管理器 + 文件解析

```c
// 配置结构
typedef struct {
    const char *name;
    guc_type_t type;
    void *value;
    const char *description;
} guc_param_t;

// 核心 API
int guc_set(const char *name, const char *value);
int guc_get(const char *name, char *buf, size_t len);
int guc_load_file(const char *path);
```

**理由**:
- PostgreSQL GUC 是事实标准，开发者熟悉
- 单例模式确保全局唯一配置视图
- 文件格式兼容 postgresql.conf，便于用户迁移

### Decision 3: SELECT 执行策略

**选择**: 火山模型（Volcano Model）+ 物理算子

```c
// 算子接口
typedef struct {
    ExecProcType  (*exec);
    ExecReScanType (*rescan);
    ExecEndType   (*end);
} ExecOps;

// 核心算子
typedef struct {
    ExecOps       ops;
    Relation      relation;
    TableScanDesc scan;
    List          *proj_targetlist;  // 投影列
    List          *qual;             // 过滤条件
} SeqScanState;
```

**理由**:
- 火山模型是数据库标准（PostgreSQL/MySQL 都用）
- 算子可组合，方便后续扩展 JOIN/AGGREGATE
- 惰性求值，按需拉取行

### Decision 4: UPDATE/DELETE 执行策略

**选择**: 标记-删除/标记-更新模式

```c
// UPDATE 执行流程
1. table_beginscan(rel)  // 打开扫描
2. while (heap_getnext(scan)) {
       if (eval_qual(expr, tuple)) {
           // 先标记旧元组为已删除
           heap_delete(rel, &tuple->ctid);
           // 构造新元组
           HeapTuple new_tuple = form_new_tuple(tuple, set_values);
           // 插入新元组
           heap_insert(rel, new_tuple);
       }
   }
3. table_endscan(scan)
```

**理由**:
- 符合 MVCC 语义（不原地修改，便于回滚）
- 实现简单，不需要处理页面空间碎片
- 与现有 WAL 机制兼容

## Risks / Trade-offs

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 浮点数 token 与 IDENT 冲突 | `3.14` 可能被误解析 | 优先检查小数点后跟数字 |
| 复杂 WHERE 表达式性能 | 深层嵌套条件可能慢 | 添加表达式缓存/短路求值 |
| UPDATE 元组版本累积 | 页面空间浪费 | 定期 VACUUM 清理 |
| 配置文件格式错误 | 参数设置失败 | 详细的错误提示和回退 |
| 多线程竞态 | 初始化时竞态 | 使用 `std::call_once` 或原子操作 |

## Migration Plan

### Phase 1: SQL 层修复（立即）
1. 修复 sql_lexer.c 添加 SQL_TOKEN_FLOAT
2. 增强 sql_parser.c 表达式解析
3. 实现 sql_exec.c 中的 SELECT/UPDATE/DELETE

### Phase 2: 配置系统（第二周）
1. 创建 db_config.h/c
2. 实现 GUC 参数管理
3. 支持 postgresql.conf 解析

### Phase 3: 启动机制（第三周）
1. 创建 initdb 工具
2. 实现服务器模式
3. 添加客户端连接处理

### 回滚策略
- 每个阶段独立提交，可单独回滚
- 配置系统有默认值，回滚后使用硬编码值
- initdb 只初始化新目录，不破坏现有数据

## Open Questions

1. **LIKE 通配符转义**: 如何处理用户输入中的 `%`、`_` 字符？
2. **字符编码**: 是否需要支持 UTF-8，还是只用 ASCII？
3. **索引利用**: SELECT 是否应该利用 BTree 索引加速？
4. **配置生效时机**: 某些参数（如 shared_buffers）修改后是否需要重启？
