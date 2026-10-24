# 数据库内核模块
# 参考 PostgreSQL 架构的模块化分层设计
#
# 层次结构：
#   core/       - 核心基础设施（errors, log, guc）
#   storage/    - 存储层（Buffer Pool, Catalog, WAL, 各引擎）
#   index/      - 索引实现
#   parser/     - 语法解析层
#   optimizer/  - 查询优化层
#   rewrite/    - 查询重写层
#   validator/  - 校验层
#   executor/   - 执行层
#   utils/      - 工具库

# ========================================================================
# 核心模块（errors, log, guc）
# ========================================================================
add_subdirectory(core)

# 共识模块（Raft 共识算法，Phase11 引入）
add_subdirectory(consensus)

# ========================================================================
# YANG 数据建模（数据模型层）
#
# 需在 storage 之前引入：yang_parser 是 OBJECT 库，其 obj 由
# db_storage 收集（见 storage/CMakeLists.txt 的 DB_STORAGE_OBJECTS），
# 否则 yang_datastore_create / yang_model_create / yang_schema_find 等
# 18 个符号在链接期缺失。yang_parser 只依赖 project_includes，
# 不依赖 storage，故提前引入是安全的。
# ========================================================================
add_subdirectory(yang)

# ========================================================================
# 存储层（Buffer Pool, Catalog, WAL, Transaction, 各引擎）
# ========================================================================
add_subdirectory(storage)

# db_core 内含 engine_registry.c，引用各存储引擎的 *_engine_get_ops 符号
# （kv_/vector_/graph_/spatial_/yang_）与 mm_storage_init / storage_register_engine
# 等，这些符号由 db_storage 收集的引擎 OBJECT 库提供。
# 需在 add_subdirectory(storage) 之后声明：core 目录先于 storage 被处理，
# 那时 db_storage 目标尚未存在，if(TARGET db_storage) 判定为假。
# db_storage 本身也链接 db_core（PUBLIC），形成静态库循环依赖 —— CMake 支持
# 这种情况，会在最终链接行中重复列出相关库。
if(TARGET db_storage)
    target_link_libraries(db_core PUBLIC db_storage)
endif()

# ========================================================================
# 索引子系统（B+Tree, Hash, Bitmap, Vector Index）
# ========================================================================
add_subdirectory(index)

# ========================================================================
# 解析层（SQL, GQL 语法解析）
# ========================================================================
# parser/CMakeLists.txt 内部按 sql/ 与 graph/ 子目录分别 add_subdirectory。
# 仅 sql 子模块受 MMDB_ENABLE_RELATIONAL 守卫；graph/GQL 留在 RELATIONAL 之外。
add_subdirectory(parser)

# ========================================================================
# SQL 引擎层（解析器 + 计划器 + 执行器）
# ========================================================================
if(MMDB_ENABLE_RELATIONAL)
    add_subdirectory(sql)
endif()

# ========================================================================
# 优化层（规则优化, 代价估算, 执行计划生成）
# ========================================================================
add_subdirectory(optimizer)

# ========================================================================
# 重写层（查询重写）
# ========================================================================
add_subdirectory(rewrite)

# ========================================================================
# 校验层（约束检查, 语义分析）
# ========================================================================
if(MMDB_ENABLE_RELATIONAL)
    add_subdirectory(validator)
endif()

# ========================================================================
# 执行层（SQL/Graph 执行器）
# ========================================================================
add_subdirectory(executor)

# ========================================================================
# 工具库（向量索引依赖的数据结构）
# ========================================================================
add_subdirectory(utils)

# ========================================================================
# 安全模块（权限校验、审计）
# ========================================================================
add_subdirectory(security)

# ========================================================================
# 其他模块
# ========================================================================

# 事务管理器（MVCC 事务管理）
add_subdirectory(txn)

# 并发控制（MVCC 多版本并发控制）
add_subdirectory(concurrency)

# 分布式模块（分片路由层）
add_subdirectory(distributed)

# Gap#2 向量化执行引擎（列存批处理算子层，依赖 db_core，须在 core 之后）
add_subdirectory(vectorized)

# 主从复制（流复制、故障切换）
add_subdirectory(replication)

# 分片支持（Hash/Range 分片、路由层）
# T11：migrate_manager.c 直接依赖 db/storage/kv/kv.h，整个 sharding/ 目录
# 必须随 KV 一起关闭，避免 KV 关闭时链接失败。
if(MMDB_ENABLE_KV)
    add_subdirectory(sharding)
endif()

# 订阅模块（CDC 变更数据捕获）
add_subdirectory(subscription)

# 对账模块（账本、账务一致性）
add_subdirectory(ledger)

# 老化模块（冷热数据分层、自动清理）
add_subdirectory(aging)

# 视图（普通视图、物化视图）
add_subdirectory(view)

# 触发器（行级/语句级触发器）
add_subdirectory(trigger)

# 后台任务调度器
add_subdirectory(bgworker)

# 性能优化（SIMD、并行查询、缓存）
add_subdirectory(performance)

# CLI 交互界面（SQL Shell + 向量 CLI）
# T11：canonical CLI（T10 落地）链接 sql_engine + db_parser_sql，
# 仅 RELATIONAL 模态下有意义；RELATIONAL 关闭时整个 cli/ 不构建。
if(MMDB_ENABLE_RELATIONAL)
    add_subdirectory(cli)
endif()

# API 层（REST API + 向量 API）
add_subdirectory(api)

# ========================================================================
# 协议兼容层（MySQL 线协议 + 分析 SQL 方言）
# ========================================================================
add_subdirectory(protocol)

# ========================================================================
# NETCONF 网络配置协议
# ========================================================================
add_subdirectory(netconf)

# ========================================================================
# 工具模块（系统表管理、数据导入导出、统计信息、执行计划分析、空间回收、索引重建）
# ========================================================================
add_subdirectory(tools)

# ========================================================================
# 列族存储引擎（基于 KV，支持动态列、宽行）
# ========================================================================
# T11：cf_engine.c 内部使用 kv_t，整个 cf/ 目录随 KV 一起关闭。
if(MMDB_ENABLE_KV)
    add_subdirectory(cf)
endif()

# ========================================================================
# 生态系统模块（REST API 服务器）
# ========================================================================
add_subdirectory(ecosystem)
