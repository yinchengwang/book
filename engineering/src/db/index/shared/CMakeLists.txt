# 树索引共享基础设施
add_library(db_tree_shared STATIC
    tree_page.c
)

target_include_directories(db_tree_shared PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/index
)

target_include_directories(db_tree_shared PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
