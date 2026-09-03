# storage/catalog 模块：系统目录
file(GLOB_RECURSE CATALOG_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(CATALOG_SOURCE_FILES)
    add_library(storage_catalog OBJECT ${CATALOG_SOURCE_FILES})
    target_include_directories(storage_catalog PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
endif()
