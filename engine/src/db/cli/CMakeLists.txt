# CLI 交互界面模块（canonical SQL 路径）
#
# T10 (WS-D 出口): CLI 重接线。
#   - 链接 sql_engine（提供 execute_sql → QueryResult）
#   - 链接 db_parser_sql（sql_driver 调 sql_parse）
#   - 链接 db_storage + db_core（catalog/buf/heapam/btreeam/rel 启动序列）
#   - 不链接 vector_api（vector_cli.c 待 vector_api 恢复后另行接入）
#   - multimodel_config_gen 提供 MMDB_ENABLE_* 模态宏

add_library(db_cli_lib STATIC cli.c)

target_include_directories(db_cli_lib PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)

target_link_libraries(db_cli_lib PUBLIC
    sql_engine
    db_storage
    db_parser_sql
    db_core
    project_includes
    multimodal_config_gen
)

# CLI 可执行程序
add_executable(db_cli cli_main.c)
target_link_libraries(db_cli PRIVATE db_cli_lib)
