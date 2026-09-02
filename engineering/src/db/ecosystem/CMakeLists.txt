# Ecosystem 模块
# 提供生态系统相关的功能

# Plugin manager
add_library(ecosystem_plugin_manager STATIC plugin_manager.c)
target_include_directories(ecosystem_plugin_manager PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/ecosystem
)

# REST API 服务器
add_library(ecosystem_rest_server STATIC rest_server.c)
target_include_directories(ecosystem_rest_server PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/ecosystem
)
