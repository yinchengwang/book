# 流式存储引擎模块

add_library(storage_stream OBJECT
    stream_engine.c
)

target_include_directories(storage_stream PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(storage_stream PRIVATE -Wno-unused-parameter)
