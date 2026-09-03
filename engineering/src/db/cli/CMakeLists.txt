# CLI 交互界面模块 - 暂时禁用（依赖已禁用的 vector_api 模块）
# file(GLOB_RECURSE CLI_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

# if(CLI_SOURCE_FILES)
#     # 创建 CLI 库（包含 cli.c 和 vector_cli.c）
#     add_library(db_cli_lib STATIC ${CLI_SOURCE_FILES})
#     target_include_directories(db_cli_lib PUBLIC
#         ${ENGINEERING_SOURCE_DIR}/include
#         ${ENGINEERING_SOURCE_DIR}/include/db
#         ${ENGINEERING_SOURCE_DIR}/include/db/cli
#         ${ENGINEERING_SOURCE_DIR}/include/db/api
#     )
#     target_link_libraries(db_cli_lib PUBLIC
#         db_storage
#         db_parser
#         db_executor
#         db_utils
#         vector_api
#         db_core
#         project_includes
#     )

#     # CLI 可执行程序
#     add_executable(db_cli cli_main.c)
#     target_include_directories(db_cli PRIVATE
#         ${ENGINEERING_SOURCE_DIR}/include
#         ${ENGINEERING_SOURCE_DIR}/include/db
#         ${ENGINEERING_SOURCE_DIR}/include/db/cli
#     )
#     target_link_libraries(db_cli PRIVATE
#         db_cli_lib
#         db_storage
#         db_parser
#         db_executor
#         db_utils
#         project_includes
#     )
# endif()
