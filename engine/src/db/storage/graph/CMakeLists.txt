# storage/graph 模块：图存储引擎
file(GLOB_RECURSE GRAPH_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

# 包含遍历算法源文件
set(TRAVERSE_SOURCE_FILES
    ${ENGINEERING_SOURCE_DIR}/src/db/executor/graph/traverse/traverse.c
)

# 包含图分析算法和 Cypher 解析器源文件
set(GRAPH_ALGO_SOURCE_FILES
    ${ENGINEERING_SOURCE_DIR}/src/db/graph/graph_algorithms.c
    ${ENGINEERING_SOURCE_DIR}/src/db/graph/graph_cypher.c
    ${ENGINEERING_SOURCE_DIR}/src/db/graph/graph_sql_functions_register.c  # Task 3: 图模态 SQL 函数表
)

if(GRAPH_SOURCE_FILES OR TRAVERSE_SOURCE_FILES OR GRAPH_ALGO_SOURCE_FILES)
    # Task 9：独立静态库（模板见 storage/kv）。graph 白名单依赖 kv。
    add_library(mm_graph STATIC ${GRAPH_SOURCE_FILES} ${TRAVERSE_SOURCE_FILES} ${GRAPH_ALGO_SOURCE_FILES})
    target_include_directories(mm_graph PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        # 原为 ${CMAKE_SOURCE_DIR}/third_part/uthash/src —— 该路径不存在
        # （CMAKE_SOURCE_DIR 是仓库根，根下 vendor/third_part/uthash 是空 submodule），
        # 一直是空挂。改用工程内实体路径。
        ${ENGINEERING_SOURCE_DIR}/third_part/uthash/src
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
        ${ENGINEERING_SOURCE_DIR}/include/db/executor
        ${ENGINEERING_SOURCE_DIR}/src/db/executor/graph
        ${ENGINEERING_SOURCE_DIR}/src/db/graph
    )
    # L1 设施（nm 实测：wal/lock/mm_pool）+ 依赖闭包 kv + L0
    target_link_libraries(mm_graph PUBLIC mm_base mm_kv storage_wal storage_lock storage_mm)
    set_target_properties(mm_graph PROPERTIES FOLDER "ModalityEngines")
endif()

# 单独编译的模块（使用 GLOB_RECURSE 会自动包含）
# - graph_csr.c: CSR 存储格式
# - graph_dedup.c: 图去重
