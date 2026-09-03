# HNSW-SQ 混合索引库

add_library(hnsw_sq STATIC hnsw_sq.c)

target_include_directories(hnsw_sq PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(hnsw_sq PRIVATE project_includes Threads::Threads)