# 分布式模块
# Phase E2: 分片路由层
# Phase E3: 分布式查询执行
# Phase E4: RPC 通信

# 分片路由模块（始终构建）
add_subdirectory(shard)

# RPC 模块（始终构建）
add_subdirectory(rpc)


# 分布式事务增量模块（Gap#1, 骨架）
add_subdirectory(tso)
add_subdirectory(mvcc_ts)
add_subdirectory(percolator)
add_subdirectory(deadlock)

# 条件编译: 仅在启用分布式能力时构建查询执行
if(MMDB_ENABLE_DISTRIBUTED)
    # 查询执行子模块
    add_subdirectory(query)
endif()
