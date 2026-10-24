# 分布式 HNSW 索引模块
add_library(dist_hnsw OBJECT
    dist_hnsw.c
)

target_include_directories(dist_hnsw PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/index/vector_index/dist_hnsw
)

target_link_libraries(dist_hnsw PRIVATE
    db_core
)

target_compile_options(dist_hnsw PRIVATE -Wno-unused-parameter)

set_target_properties(dist_hnsw PROPERTIES FOLDER "Libraries")
