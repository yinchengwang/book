# HNSW-PQ 混合索引

file(GLOB HNSW_PQ_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

add_library(hnsw_pq STATIC ${HNSW_PQ_SOURCES})

target_include_directories(hnsw_pq PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(hnsw_pq PUBLIC
    project_includes
    pq
    # hnsw_pq.c 通过 db/index/heap/heap_vector_store.h 调用
    # heap_vector_get / heap_vector_insert / heap_vector_store_*。
    # 该库此前未声明此依赖，依赖目标自行链接（hnsw_pq_test 链了，
    # test_vector_index 没链）→ 链接期 undefined reference。
    # 依赖应由提供方声明，故在此补齐。
    heap_vector_store
)