# T-Tree 索引库
file(GLOB_RECURSE TTREE_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(TTREE_SOURCE_FILES)
    add_library(db_ttree STATIC ${TTREE_SOURCE_FILES})
    target_include_directories(db_ttree PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
    )
endif()
