# C 游戏服务器（基于 libmicrohttpd）
# 依赖：games_core、microhttpd
# T12：libmicrohttpd 头文件未安装（third_party/libmicrohttpd/include 缺失）；
# 不阻塞 SQL 验收。暂时排除。
# add_executable(games_server
#     main.c
#     session.c
# )
#
# target_include_directories(games_server PRIVATE
#     ${CMAKE_SOURCE_DIR}/apps/games/core
#     ${CMAKE_SOURCE_DIR}/third_party/libmicrohttpd/include
# )
#
# target_link_libraries(games_server PRIVATE
#     project_includes
#     games_core
#     microhttpd
# )
