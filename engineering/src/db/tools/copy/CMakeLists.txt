# 数据导入导出工具
include(${CMAKE_SOURCE_DIR}/cmake/ProjectUtils.cmake)

add_project_library(db_tools_copy
    copy.c
    copy_options.c
    copy_csv.c
    copy_text.c
    copy_json.c
    copy_sql.c
    copy_binary.c
)

target_include_directories(db_tools_copy PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/tools
)