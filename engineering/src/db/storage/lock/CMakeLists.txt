# storage/lock 模块：锁管理器
file(GLOB_RECURSE LOCK_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(LOCK_SOURCE_FILES)
    add_library(storage_lock OBJECT ${LOCK_SOURCE_FILES})
    target_include_directories(storage_lock PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
