# 统计信息工具
include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_library(db_tools_stats
    stats.c
    stat_views.c
)
target_include_directories(db_tools_stats PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/tools
)
