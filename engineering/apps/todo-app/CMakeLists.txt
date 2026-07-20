# Todo App - 待办事项应用
# 全功能待办事项管理系统（JSON 文件持久化）

file(GLOB TODO_APP_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*.c"
)

add_executable(todo-app ${TODO_APP_SOURCES})

target_include_directories(todo-app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/../third_part
)

# 添加 sqlite3 子目录
# 添加 sqlite3 子目录
add_subdirectory(${ENGINEERING_SOURCE_DIR}/../third_part/sqlite3 ${CMAKE_BINARY_DIR}/sqlite3)

target_link_libraries(todo-app PRIVATE
    project_includes
    cjson
    sqlite3
    ws2_32
)
