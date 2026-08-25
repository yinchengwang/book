# 多模态数据库编译配置
# 定义所有模态开关和分布式能力开关
# 由 CMake configure_file() 生成 multimodal_config.h

# ========================================================================
# 顶层模态开关（默认全开，用户可按需关闭以裁剪构建）
# ========================================================================

option(MMDB_ENABLE_RELATIONAL "Enable Relational Model" ON)
option(MMDB_ENABLE_KV "Enable KV Model" ON)
option(MMDB_ENABLE_GRAPH "Enable Graph Model" ON)
option(MMDB_ENABLE_VECTOR "Enable Vector Model" ON)
option(MMDB_ENABLE_TIMESERIES "Enable Timeseries Model" ON)
option(MMDB_ENABLE_DOCUMENT "Enable Document Model" ON)
option(MMDB_ENABLE_SPATIAL "Enable Spatial Model" ON)
option(MMDB_ENABLE_TREE "Enable Tree Model" ON)
option(MMDB_ENABLE_STREAM "Enable Stream Model" OFF)  # 新增，默认关闭
option(MMDB_ENABLE_COLUMNAR "Enable Columnar Model" OFF)  # 新增，默认关闭

# ========================================================================
# 分布式能力开关
# ========================================================================

option(MMDB_ENABLE_DISTRIBUTED "Enable Distributed Features" OFF)
option(MMDB_ENABLE_DISTRIBUTED_RAFT "Enable Raft Consensus" OFF)
option(MMDB_ENABLE_DISTRIBUTED_SHARD "Enable Sharding" OFF)

# ========================================================================
# 配置头生成
# ========================================================================

set(MULTIMODAL_CONFIG_HEADER "${ENGINEERING_SOURCE_DIR}/include/db/multimodal_config.h")
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/multimodal_config.h.in"
    "${MULTIMODAL_CONFIG_HEADER}"
)

# ========================================================================
# 打印配置状态
# ========================================================================

message(STATUS "Multimodal Config:")
message(STATUS "  Relational: ${MMDB_ENABLE_RELATIONAL}")
message(STATUS "  KV: ${MMDB_ENABLE_KV}")
message(STATUS "  Graph: ${MMDB_ENABLE_GRAPH}")
message(STATUS "  Vector: ${MMDB_ENABLE_VECTOR}")
message(STATUS "  Timeseries: ${MMDB_ENABLE_TIMESERIES}")
message(STATUS "  Document: ${MMDB_ENABLE_DOCUMENT}")
message(STATUS "  Spatial: ${MMDB_ENABLE_SPATIAL}")
message(STATUS "  Tree: ${MMDB_ENABLE_TREE}")
message(STATUS "  Stream: ${MMDB_ENABLE_STREAM}")
message(STATUS "  Columnar: ${MMDB_ENABLE_COLUMNAR}")
message(STATUS "  Distributed: ${MMDB_ENABLE_DISTRIBUTED}")
