# B-Tree 索引库
file(GLOB_RECURSE BTREE_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(BTREE_SOURCE_FILES)
    add_library(db_btree STATIC ${BTREE_SOURCE_FILES})
    target_include_directories(db_btree PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
    )
    target_link_libraries(db_btree PRIVATE db_tree_shared)
endif()
