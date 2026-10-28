# 列式存储引擎模块
# Task 11：独立静态库；columnar_serialize_table 前置声明已修（原静态定义
# 在调用点之后且无前向声明，从未被编译过）

add_library(mm_columnar STATIC
    columnar_engine.c
)

target_include_directories(mm_columnar PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mm_columnar PUBLIC mm_base)
set_target_properties(mm_columnar PROPERTIES FOLDER "ModalityEngines")
target_compile_options(mm_columnar PRIVATE -Wno-unused-parameter)
