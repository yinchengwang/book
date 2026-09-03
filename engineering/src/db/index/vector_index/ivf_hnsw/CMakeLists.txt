# IVF-HNSW 混合索引库

add_library(ivf_hnsw STATIC ivf_hnsw.c)

target_include_directories(ivf_hnsw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(ivf_hnsw PRIVATE project_includes Threads::Threads)
