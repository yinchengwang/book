# R-Tree 索引库
file(GLOB_RECURSE RTREE_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(RTREE_SOURCE_FILES)
    add_library(db_rtree STATIC ${RTREE_SOURCE_FILES})
    target_include_directories(db_rtree PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
    )
endif()
