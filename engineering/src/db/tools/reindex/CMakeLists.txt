# 索引重建工具
include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_library(db_tools_reindex reindex.c)
target_include_directories(db_tools_reindex PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/tools
)
