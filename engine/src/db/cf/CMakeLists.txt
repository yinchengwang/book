# 列族（Column Family）存储引擎模块

add_library(cf_engine OBJECT
    cf_column.c
    cf_row.c
    cf_engine.c
)

target_include_directories(cf_engine PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/cf
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cf_engine PUBLIC
    db_storage
    db_core
)

target_compile_options(cf_engine PRIVATE -Wno-unused-parameter)