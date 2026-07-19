# Access Method 模块（索引 AM 框架）
add_library(access_amapi STATIC
    amapi.c
    am_btree.c
    am_hnsw.c
)

target_include_directories(access_amapi PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)

target_link_libraries(access_amapi PUBLIC db_core db_storage)