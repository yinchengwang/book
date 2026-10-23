# 算法库（生产版）—— 提供 K-Means、量化、距离、二分查找、分词等
# 工程性算法，不包含学习内容（学习内容归 learning/algo-c/）

file(GLOB_RECURSE ALGO_PROD_SOURCE_FILE CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

# dict/ 分词模块：同时被 BM25 索引（engineering）和单元测试使用，保留在工程层
# S6 排除学习性内容：distributed/impl/（依赖缺失的头文件 ds/distributed_index.h）
list(FILTER ALGO_PROD_SOURCE_FILE EXCLUDE REGEX "^distributed/impl/")
list(FILTER ALGO_PROD_SOURCE_FILE EXCLUDE REGEX "^binary_search/problems/")

add_library(algo-prod STATIC ${ALGO_PROD_SOURCE_FILE})

target_include_directories(algo-prod PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
    # 原先跨轨引用 learning 轨的头文件目录；learning 切出独立仓后改用工程内
    # third_part/uthash/src（该目录直接存放 uthash.h）。
    ${ENGINEERING_SOURCE_DIR}/third_part/uthash/src
)

target_link_libraries(algo-prod
    PRIVATE
        project_includes
)

set_target_properties(algo-prod PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    FOLDER "Libraries"
)

message(STATUS "ALGO_PROD_SOURCE_FILE: ${ALGO_PROD_SOURCE_FILE}")
