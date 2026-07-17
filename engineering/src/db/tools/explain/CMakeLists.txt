# 执行计划分析工具
include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_library(db_tools_explain
    explain.c
    explain_text.c
    explain_json.c
    explain_yaml.c
)
target_include_directories(db_tools_explain PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/tools
)
