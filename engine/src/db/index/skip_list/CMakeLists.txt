# Skip List 索引库
file(GLOB_RECURSE SKIP_LIST_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(SKIP_LIST_SOURCE_FILES)
    add_library(db_skip_list STATIC ${SKIP_LIST_SOURCE_FILES})
    target_include_directories(db_skip_list PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
    )
endif()
