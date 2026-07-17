# storage/graph 模块：图存储引擎
file(GLOB_RECURSE GRAPH_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

# 包含遍历算法源文件
set(TRAVERSE_SOURCE_FILES
    ${ENGINEERING_SOURCE_DIR}/src/db/executor/graph/traverse/traverse.c
)

if(GRAPH_SOURCE_FILES OR TRAVERSE_SOURCE_FILES)
    add_library(storage_graph OBJECT ${GRAPH_SOURCE_FILES} ${TRAVERSE_SOURCE_FILES})
    target_include_directories(storage_graph PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${ENGINEERING_SOURCE_DIR}/src/db/executor/graph
    )
endif()

# 单独编译的模块（使用 GLOB_RECURSE 会自动包含）
# - graph_csr.c: CSR 存储格式
# - graph_dedup.c: 图去重
