# ReRanker 模块
# 两阶段检索中的精排组件

file(GLOB_RECURSE VECTOR_RERANKER_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

add_library(vector_reranker STATIC ${VECTOR_RERANKER_SOURCES})

target_include_directories(vector_reranker PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
    ${ENGINEERING_SOURCE_DIR}/src/db/index/vector_index
    ${ENGINEERING_SOURCE_DIR}/src/db/index/vector_index/hnsw
)

target_link_libraries(vector_reranker PRIVATE project_includes algo-prod Threads::Threads)

# 导出变量供测试使用
set(VECTOR_RERANKER_LIBRARY vector_reranker PARENT_SCOPE)
