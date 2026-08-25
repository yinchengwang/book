# 列式存储引擎模块

add_library(storage_columnar OBJECT
    columnar_engine.c
)

target_include_directories(storage_columnar PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(storage_columnar PRIVATE -Wno-unused-parameter)
