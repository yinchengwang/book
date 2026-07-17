# 向量 Heap 主存储
# 解决向量膨胀：Heap 持有完整向量，索引仅持 vector_ref_t

include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_library(heap_vector_store HEAP_VECTOR_STORE_SOURCES
)

# heap_vector_store.h 位于 include/db/index/heap/，需要显式添加搜索路径
target_include_directories(heap_vector_store
    PRIVATE
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
)
