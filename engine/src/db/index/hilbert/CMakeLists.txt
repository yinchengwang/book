# Hilbert 曲线索引模块
add_library(index_hilbert STATIC
    hilbert.c
)
target_include_directories(index_hilbert PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/core
)
