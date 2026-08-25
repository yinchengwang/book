# DiskANN module
# Vamana 图索引实现（Disk-based Approximate Nearest Neighbor）

# 源文件列表
set(DISKANN_SOURCES
    diskann_config.c
    diskann_create.c
    diskann_delete.c
    diskann_disk_io.c
    diskann_graph.c
    diskann_insert.c
    diskann_persist.c
    diskann_pq.c
    diskann_quantization.c
    diskann_search.c
    diskann_stubs.c
    diskann_utils.c
    diskann_vamana.c
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