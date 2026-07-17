# 数据库内核工具库
# 包含向量索引依赖的数据结构（堆、访问表等）

file(GLOB_RECURSE DB_UTILS_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(DB_UTILS_SOURCE_FILES)
    add_library(db_utils STATIC ${DB_UTILS_SOURCE_FILES})
    target_include_directories(db_utils PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${ENGINEERING_SOURCE_DIR}/include
        ${ENGINEERING_SOURCE_DIR}/include/db
        ${ENGINEERING_SOURCE_DIR}/include/db/utils
    )
endif()
