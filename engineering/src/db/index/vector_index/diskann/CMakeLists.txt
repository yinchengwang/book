# DiskANN module
# Vamana 图索引实现（Disk-based Approximate Nearest Neighbor）

# 源文件列表（仅 Task 1.3 基准测试所需：create/add/build/search/drop + 依赖项）
# 注：diskann_spann / diskann_merge / diskann_fresh / diskann_partition
# 等模块当前未通过编译验证（API 与 distance.h 不一致、私有头与公有头冲突），
# 留待后续任务单独修复。本测试仅覆盖基础 Vamana 图构建/搜索路径。
set(DISKANN_SOURCES
    diskann_config.c
    diskann_create.c
    diskann_delete.c
    diskann_graph.c
    diskann_insert.c
    diskann_persist.c
    diskann_quantization.c
    diskann_search.c
    diskann_stubs.c
    diskann_utils.c
)

# 创建静态库
add_library(diskann STATIC ${DISKANN_SOURCES})

target_include_directories(diskann PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/index/vector_index/diskann
)

target_link_libraries(diskann PRIVATE
    project_includes
    algo-prod
    heap_vector_store
    storage_backend_common
    Threads::Threads
)