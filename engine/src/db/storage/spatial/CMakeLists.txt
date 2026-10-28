# storage/spatial 模块：空间存储引擎
file(GLOB_RECURSE SPATIAL_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(SPATIAL_SOURCE_FILES)
    add_library(mm_spatial STATIC ${SPATIAL_SOURCE_FILES})
    target_include_directories(mm_spatial PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
    # Task 10：索引设施（hilbert/rtree，nm 实测）+ L0；wal 由 L1 清单提供
    target_link_libraries(mm_spatial PUBLIC mm_base index_hilbert db_rtree storage_wal storage_page)
    set_target_properties(mm_spatial PROPERTIES FOLDER "ModalityEngines")
endif()
