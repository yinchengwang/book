# storage/yang 模块：树存储引擎
file(GLOB_RECURSE YANG_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(YANG_SOURCE_FILES)
    add_library(storage_yang OBJECT ${YANG_SOURCE_FILES})
    target_include_directories(storage_yang PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
