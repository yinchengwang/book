# 自动收集当前目录下所有 .c 源文件
file(GLOB SNAKE_SOURCES CONFIGURE_DEPENDS "*.c")
add_executable(snake_game ${SNAKE_SOURCES})

target_link_libraries(snake_game PRIVATE project_includes)

set_target_properties(snake_game PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
