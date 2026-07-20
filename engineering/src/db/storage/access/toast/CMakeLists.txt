# storage/access/toast 模块：TOAST 大值存储
file(GLOB_RECURSE TOAST_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(TOAST_SOURCE_FILES)
    add_library(storage_access_toast OBJECT ${TOAST_SOURCE_FILES})
    target_include_directories(storage_access_toast PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()