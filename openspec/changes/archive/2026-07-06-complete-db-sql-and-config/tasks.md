# SQL 解析和配置系统实现任务

## 1. SQL 词法分析器修复

- [x] 1.1 在 `sql_token_type_t` 中添加 `SQL_TOKEN_FLOAT` 枚举值
- [x] 1.2 在 `sql_lexer.c` 中实现浮点数 token 识别（支持小数和指数）
- [x] 1.3 在 `sql_lexer.c` 中更新 token 值存储（添加 `float_value` 字段）
- [ ] 1.4 添加浮点数解析单元测试

## 2. SQL 表达式解析增强

- [x] 2.1 添加新的 AST 节点类型（LOGICAL_OP, IN_LIST, BETWEEN, LIKE, IS_NULL）
- [x] 2.2 实现操作符优先级解析（支持 AND/OR/NOT）
- [x] 2.3 实现比较操作符解析（=, <>, >, >=, <, <=）
- [x] 2.4 实现 LIKE/IN/BETWEEN 解析
- [x] 2.5 实现 IS NULL/IS TRUE/IS FALSE 解析
- [x] 2.6 实现括号优先级处理
- [ ] 2.7 添加表达式解析单元测试

## 3. SELECT 执行器实现

- [x] 3.1 定义执行节点基础结构（ExecNode, EState）
- [x] 3.2 实现 SeqScanState（顺序扫描算子）
- [x] 3.3 实现 FilterState（过滤算子）
- [x] 3.4 实现 ProjectionState（投影算子）
- [x] 3.5 实现条件表达式求值（eval_expr）
- [x] 3.6 实现 `sql_executor_select()` 函数
- [x] 3.7 添加 SELECT 执行集成测试

## 4. UPDATE 执行器实现

- [x] 4.1 定义 UpdateState 结构
- [x] 4.2 实现 form_new_tuple（根据 SET 子句构造新元组）
- [x] 4.3 实现 update_tuple（单行更新）
- [x] 4.4 实现 exec_update（批量更新）
- [x] 4.5 集成 WAL 记录更新前后状态
- [x] 4.6 添加 UPDATE 执行集成测试

## 5. DELETE 执行器实现

- [x] 5.1 定义 DeleteState 结构
- [x] 5.2 实现 heap_delete（标记元组为已删除）
- [x] 5.3 实现 exec_delete（批量删除）
- [x] 5.4 处理 MVCC 可见性检查
- [x] 5.5 集成 WAL 记录删除状态
- [x] 5.6 添加 DELETE 执行集成测试

## 6. GUC 配置系统实现

- [ ] 6.1 定义 guc_param_t 和 guc_type_t
- [ ] 6.2 实现 guc_init（初始化参数表）
- [ ] 6.3 实现 guc_set（设置参数值）
- [ ] 6.4 实现 guc_get（获取参数值）
- [ ] 6.5 实现 guc_reset（重置参数）
- [ ] 6.6 实现配置文件解析器（postgresql.conf）
- [ ] 6.7 实现单位转换（MB, GB, kB, s, min 等）
- [ ] 6.8 注册核心配置参数（shared_buffers, work_mem, wal_level 等）
- [ ] 6.9 添加 GUC 系统单元测试

## 7. initdb 工具实现

- [ ] 7.1 定义 initdb_options_t 和 initdb 函数
- [ ] 7.2 实现目录结构创建
- [ ] 7.3 实现系统表初始化（pg_database, pg_class 等）
- [ ] 7.4 实现模板数据库创建
- [ ] 7.5 实现 postgresql.conf 生成
- [ ] 7.6 实现 pg_hba.conf 生成
- [ ] 7.7 实现 WAL 初始化
- [ ] 7.8 实现权限设置
- [ ] 7.9 添加 initdb 集成测试

## 8. 数据库服务器实现

- [ ] 8.1 定义 server_options_t 和 Server 结构
- [ ] 8.2 实现 Listener（TCP/Unix Socket 监听）
- [ ] 8.3 实现 Connection 和 ConnManager
- [ ] 8.4 实现简化版 PostgreSQL wire 协议处理器
- [ ] 8.5 实现 Startup 阶段（协议版本协商）
- [ ] 8.6 实现 Simple Query 协议（Query/Response）
- [ ] 8.7 实现 ErrorResponse 格式化
- [ ] 8.8 实现 pg_ctl 工具（start/stop/reload/status）
- [ ] 8.9 添加服务器集成测试

## 9. Bug 修复

- [x] 9.1 修复 txn_mutex_init_if_needed 竞态条件（使用 std::call_once） — 不适用，代码中不存在此函数
- [x] 9.2 运行完整测试套件验证

## 10. 文档更新

- [x] 10.1 更新 CLAUDE.md 添加配置系统说明
- [x] 10.2 更新 storage-architecture.md 添加执行器架构
- [x] 10.3 添加配置文件示例（postgresql.conf）
