# storage/access/heap 模块：堆表访问方法
file(GLOB_RECURSE HEAP_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(HEAP_SOURCE_FILES)
    add_library(storage_access_heap OBJECT ${HEAP_SOURCE_FILES})
    target_include_directories(storage_access_heap PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
