# CCEH Hash 索引
file(GLOB_RECURSE CCEH_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(CCEH_SOURCE_FILES)
    add_library(db_cceh STATIC ${CCEH_SOURCE_FILES})
    target_include_directories(db_cceh PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/index
    )
endif()