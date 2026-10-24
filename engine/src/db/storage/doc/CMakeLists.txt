# storage/doc 模块：文档存储引擎
file(GLOB_RECURSE DOC_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(DOC_SOURCE_FILES)
    add_library(storage_doc OBJECT ${DOC_SOURCE_FILES})
    target_include_directories(storage_doc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
