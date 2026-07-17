# storage/kv 模块：KV 存储引擎
file(GLOB_RECURSE KV_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(KV_SOURCE_FILES)
    add_library(storage_kv OBJECT ${KV_SOURCE_FILES})
    target_include_directories(storage_kv PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
