# storage/ts 模块：时序存储引擎
file(GLOB_RECURSE TS_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(TS_SOURCE_FILES)
    add_library(storage_ts OBJECT ${TS_SOURCE_FILES})
    target_include_directories(storage_ts PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
