# storage/multimodal 模块
file(GLOB_RECURSE MM_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")
if(MM_SOURCE_FILES)
    add_library(storage_multimodal OBJECT ${MM_SOURCE_FILES})
    target_include_directories(storage_multimodal PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
    )
endif()
