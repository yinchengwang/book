# storage/log 模块：可观测日志引擎
file(GLOB_RECURSE LOG_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")
if(LOG_SOURCE_FILES)
    add_library(storage_log OBJECT ${LOG_SOURCE_FILES})
    target_include_directories(storage_log PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
