# storage/wal 模块：预写日志
file(GLOB_RECURSE WAL_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(WAL_SOURCE_FILES)
    add_library(storage_wal OBJECT ${WAL_SOURCE_FILES})
    target_include_directories(storage_wal PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
