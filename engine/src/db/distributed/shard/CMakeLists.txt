# 分片路由模块 CMakeLists.txt
# 构建 shard_routing 库

add_library(shard_routing OBJECT
    shard_routing.c
)

target_include_directories(shard_routing PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(shard_routing PRIVATE
    project_includes
)

# db_utils 的源文件可能为空（utils/ 目前只有 CMakeLists.txt），
# 此时 add_library 不会执行、目标不存在，直接链接会导致配置期报错。
if(TARGET db_utils)
    target_link_libraries(shard_routing PRIVATE db_utils)
endif()
