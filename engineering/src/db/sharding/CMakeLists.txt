# 分片支持模块
# 添加 shard_balance.c 和 shard_coordinator.c（Task 2-3 实现）
file(GLOB_RECURSE SHARD_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(SHARD_SOURCE_FILES)
    add_library(db_sharding STATIC ${SHARD_SOURCE_FILES})
    target_include_directories(db_sharding PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/sharding
    )
    target_link_libraries(db_sharding PRIVATE project_includes)
endif()