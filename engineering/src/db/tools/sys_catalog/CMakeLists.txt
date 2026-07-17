# 系统表管理工具
include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_library(db_tools_sys_catalog sys_catalog.c)
target_include_directories(db_tools_sys_catalog PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/tools
)
