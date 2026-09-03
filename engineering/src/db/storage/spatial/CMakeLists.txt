# storage/spatial 模块：空间存储引擎
file(GLOB_RECURSE SPATIAL_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(SPATIAL_SOURCE_FILES)
    add_library(storage_spatial OBJECT ${SPATIAL_SOURCE_FILES})
    target_include_directories(storage_spatial PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
    # 依赖 Hilbert 索引模块
    target_link_libraries(storage_spatial PRIVATE index_hilbert)
endif()
