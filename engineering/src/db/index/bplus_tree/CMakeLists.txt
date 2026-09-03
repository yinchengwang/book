# B+Tree 索引
file(GLOB_RECURSE BPTREE_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

# 排除 bptree_persist.c（依赖已迁移的 tree_page 模块）
list(FILTER BPTREE_SOURCE_FILES EXCLUDE REGEX "bptree_persist.c$")

if(BPTREE_SOURCE_FILES)
    add_library(db_bptree STATIC ${BPTREE_SOURCE_FILES})
    target_include_directories(db_bptree PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
        ${ENGINEERING_SOURCE_DIR}/src/db/storage
    )
    target_link_libraries(db_bptree PRIVATE db_storage)
endif()