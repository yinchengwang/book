# storage/txn 模块：事务管理
# 排除旧的 KV 风格 txn.c（与 MVCC 头文件冲突，改用 src/db/txn/txn.c）
file(GLOB_RECURSE TXN_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")
list(FILTER TXN_SOURCE_FILES EXCLUDE REGEX "txn\\.c$")

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
