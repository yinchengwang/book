# engineering/apps/workbench_server/CMakeLists.txt
# Phase 4 Task 1：workbench-server 独立二进制（端口 5801）
# Phase 4 Task 2：挂载 workbench 6 unit（card_handler / 5 业务模块）+ card_migrate
#
# 拆分：
#   - workbench_server_config：配置模块（静态库，供测试链接）
#   - workbench-server：可执行入口（链接 config + main.c + 6 workbench unit）

add_library(workbench_server_config STATIC
    src/workbench_server_config.c
)
target_include_directories(workbench_server_config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(workbench_server_config PUBLIC c_std_11)

add_executable(workbench-server
    src/main.c
    src/workbench_metrics.c
)
target_include_directories(workbench-server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ENGINEERING_SOURCE_DIR}/apps/common
    ${ENGINEERING_SOURCE_DIR}/../third_part
    ${ENGINEERING_SOURCE_DIR}/apps/web/server/src/modules/workbench)
target_link_libraries(workbench-server PUBLIC
    apps_common
    cjson
    sqlite3
    ws2_32
    workbench_server_config
    card_storage
    card_core
    card_handler
    card_stats
    bug_core
    learning_core
)
target_compile_features(workbench-server PUBLIC c_std_11)
