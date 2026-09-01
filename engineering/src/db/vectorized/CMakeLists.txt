# Gap#2 向量化执行引擎（列存批处理算子层，链接 db_core）
add_library(db_vectorized STATIC
    vecx_block.c
    vecx_filter.c
    vecx_agg.c
    vecx_hashagg.c
    vecx_hashjoin.c
)
target_include_directories(db_vectorized PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db/vectorized
)
target_link_libraries(db_vectorized PUBLIC db_core)
target_compile_options(db_vectorized PRIVATE -Wno-unused-parameter)
set_target_properties(db_vectorized PROPERTIES FOLDER "Libraries")
