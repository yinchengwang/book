# 分布式模块
# Phase E2: 分片路由层
# Phase E3: 分布式查询执行

# 分片路由模块（始终构建）
add_subdirectory(shard)

# 条件编译: 仅在启用分布式能力时构建查询执行
if(MMDB_ENABLE_DISTRIBUTED)
    # 查询执行子模块
    add_subdirectory(query)
endif()
