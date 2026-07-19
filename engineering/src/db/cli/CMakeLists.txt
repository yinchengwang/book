# CLI 交互界面模块

# 创建 CLI 库
add_library(db_cli_lib STATIC
    cli.c
)

target_include_directories(db_cli_lib PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/cli
)

target_link_libraries(db_cli_lib PUBLIC
    db_storage
    db_parser_sql_simplified
    sql_engine
    db_core
    project_includes
)

# CLI 可执行程序
add_executable(db_cli cli_main.c)

target_include_directories(db_cli PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/cli
)

target_link_libraries(db_cli PRIVATE
    db_cli_lib
    db_storage
    db_parser_sql_simplified
    sql_engine
    db_core
    project_includes
)