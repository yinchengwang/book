# storage/txn 模块：事务管理
file(GLOB_RECURSE TXN_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(TXN_SOURCE_FILES)
    add_library(storage_txn OBJECT ${TXN_SOURCE_FILES})
    target_include_directories(storage_txn PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
