# storage/mm 模块：多模态存储管理
file(GLOB_RECURSE MM_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(MM_SOURCE_FILES)
    add_library(storage_mm OBJECT ${MM_SOURCE_FILES})
    target_include_directories(storage_mm PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/storage
        ${ENGINEERING_SOURCE_DIR}/include/db/core
    )
    # mm_storage.c 经 engine_registry.h 包含 db/multimodal_config.h（build 树生成版）
    target_link_libraries(storage_mm PRIVATE multimodal_config_gen)
endif()
