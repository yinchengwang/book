# 分片路由模块 CMakeLists.txt
# 构建 shard_routing 库

add_library(shard_routing OBJECT
    shard_routing.c
)

target_include_directories(shard_routing PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(shard_routing PRIVATE
    db_utils
)
