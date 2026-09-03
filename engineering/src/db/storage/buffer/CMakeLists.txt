# storage/buffer 模块：Buffer Pool
file(GLOB_RECURSE BUFFER_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(BUFFER_SOURCE_FILES)
    add_library(storage_buffer OBJECT ${BUFFER_SOURCE_FILES})
    target_include_directories(storage_buffer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
