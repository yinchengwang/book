# Bitmap 索引
file(GLOB_RECURSE BITMAP_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(BITMAP_SOURCE_FILES)
    add_library(db_bitmap STATIC ${BITMAP_SOURCE_FILES})
    target_include_directories(db_bitmap PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
    )
    target_link_libraries(db_bitmap PRIVATE db_storage)
endif()