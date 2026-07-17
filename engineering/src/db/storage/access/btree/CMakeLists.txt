# storage/access/btree 模块：BTree 访问方法
file(GLOB_RECURSE BTREE_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(BTREE_SOURCE_FILES)
    add_library(storage_access_btree OBJECT ${BTREE_SOURCE_FILES})
    target_include_directories(storage_access_btree PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
