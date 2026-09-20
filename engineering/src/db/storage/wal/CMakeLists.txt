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
    # Workaround: disable WAL by default (pwrite issues on some filesystems / NTFS via WSL2).
    # Override with -DDB_NO_WAL=0 to re-enable.
    # target_compile_definitions(storage_wal PUBLIC DB_NO_WAL=1)
endif()
