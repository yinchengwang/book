# 时空存储引擎模块

add_library(storage_st OBJECT
    st_engine.c
)

target_include_directories(storage_st PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(storage_st PRIVATE -Wno-unused-parameter)
