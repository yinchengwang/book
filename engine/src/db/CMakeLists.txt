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

# ========================================================================
# 注册表反转：按 profile 生成 mm_builtin_engines.c（内置引擎注册收集器）
# 必须无条件调用 —— 空 profile 也产出空函数体，否则 engine_registry_init
# 链接失败。过渡期 mm_<modality>_register 仍在 db_core 中，Task 6 抽出
# mm_storage_api 后改为模态库提供。
# ========================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/gen_builtin_engines.cmake)
mmdb_generate_builtin_engines()

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

# mm_storage_api（经 db_core 传递）内含 engine_registry.c 与生成的注册收集器，
# 引用各引擎的
# *_engine_get_ops / mm_*_register_sql_functions 与 storage_register_engine 等
# db_storage 符号。Task 4 曾尝试删除本链接，实测 5 个测试目标因
# 「单遍 archive 扫描无法满足 db_core -> db_storage 反向引用」而链接失败
# （nm -u 确认：kv_engine_get_ops、storage_register_engine 等只能由
# db_storage 提供）。断环的正确定义点是 Task 6：mm_storage_api 抽出到
# 公共层、mm_<modality>_register 随 profile 模态库提供之后，db_core
# 才可能不依赖 db_storage。届时删除本块。
# db_storage 也链接 db_core（PUBLIC），形成静态库循环依赖 —— CMake 支持，
# 会在最终链接行中重复列出相关库。
# Task 6：db_core -> L1 公共层（engine_registry + mm_storage + 注册收集器）
if(TARGET mm_storage_api)
    target_link_libraries(db_core PUBLIC mm_storage_api)
endif()

if(TARGET db_storage)
    target_link_libraries(db_core PUBLIC db_storage)
endif()

# 过渡期：mm_storage_api 的收集器引用启用模态的 *_engine_get_ops /
# mm_<modality>_register_sql_functions。若提供方不在循环 SCC 内，
# mm_storage_api 只在链接行末尾出现一次，引用无法回查（实测 undefined）。
# 已独立的模态（mm_kv）直接链；仍编在聚合内的模态经 db_storage。
# 全部模态独立后（Task 15），本段与 db_core->db_storage 一起删除。
if(TARGET mm_kv)
    target_link_libraries(mm_storage_api PRIVATE mm_kv)
endif()
if(TARGET mm_vector)
    target_link_libraries(mm_storage_api PRIVATE mm_vector)
endif()
if(TARGET mm_graph)
    target_link_libraries(mm_storage_api PRIVATE mm_graph)
endif()
if(TARGET mm_spatial)
    target_link_libraries(mm_storage_api PRIVATE mm_spatial)
endif()
if(TARGET mm_yang)
    target_link_libraries(mm_storage_api PRIVATE mm_yang)
endif()
if(TARGET mm_sparse)
    target_link_libraries(mm_storage_api PRIVATE mm_sparse)
endif()
if(TARGET mm_stream)
    target_link_libraries(mm_storage_api PRIVATE mm_stream)
endif()
if(TARGET mm_columnar)
    target_link_libraries(mm_storage_api PRIVATE mm_columnar)
endif()
set(_aggregated_engines_needed OFF)
foreach(_m ST RDF DOCUMENT TIMESERIES)
    if(MMDB_ENABLE_${_m})
        set(_aggregated_engines_needed ON)
    endif()
endforeach()
if(_aggregated_engines_needed AND TARGET db_storage)
    target_link_libraries(mm_storage_api PRIVATE db_storage)
endif()

# 生成的注册收集器直接编入 mm_storage_api（mm_<modality>_register 定义于
# engine_registry.c，同库内解析；做成独立库会打穿循环档案重复深度，
# 见 cmake/gen_builtin_engines.cmake 注释）。
if(EXISTS "${CMAKE_BINARY_DIR}/generated/src/mm_builtin_engines.c")
    target_sources(mm_storage_api PRIVATE "${CMAKE_BINARY_DIR}/generated/src/mm_builtin_engines.c")
endif()

# ========================================================================
# 索引子系统（B+Tree, Hash, Bitmap, Vector Index）
# ========================================================================
add_subdirectory(index)

# db_storage 收集了 spatial 引擎的 obj（storage_spatial），而 spatial 依赖
# index_hilbert 库的 hilbert_index_* 符号。
# 需在 add_subdirectory(index) 之后声明：storage 目录先于 index 被处理，
# 那时 index_hilbert 目标尚不存在，if(TARGET index_hilbert) 判定为假。
if(TARGET index_hilbert)
    target_link_libraries(db_storage PUBLIC index_hilbert)
endif()

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
