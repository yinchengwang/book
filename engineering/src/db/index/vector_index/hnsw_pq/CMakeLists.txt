# HNSW-PQ 混合索引

file(GLOB HNSW_PQ_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

add_library(hnsw_pq STATIC ${HNSW_PQ_SOURCES})

target_include_directories(hnsw_pq PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(hnsw_pq PUBLIC
    project_includes
    pq
)