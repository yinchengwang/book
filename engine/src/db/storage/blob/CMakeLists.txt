# Blob 存储引擎
file(GLOB_RECURSE BLOB_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")
if(BLOB_SOURCE_FILES)
    add_library(storage_blob OBJECT ${BLOB_SOURCE_FILES})
    target_include_directories(storage_blob PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/core
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
    )
endif()
