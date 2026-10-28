# 流式存储引擎模块
# Task 11：独立静态库（模板见 storage/kv）

add_library(mm_stream STATIC
    stream_engine.c
)

target_include_directories(mm_stream PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mm_stream PUBLIC mm_base)
set_target_properties(mm_stream PROPERTIES FOLDER "ModalityEngines")
target_compile_options(mm_stream PRIVATE -Wno-unused-parameter)
