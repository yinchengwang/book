# 游戏小程序（统一入口）

# 收集所有源文件
file(GLOB MINI_PROGRAM_SOURCES CONFIGURE_DEPENDS
    "*.c"
)

# 创建可执行文件
add_executable(game_center ${MINI_PROGRAM_SOURCES})

# 链接公共库
target_link_libraries(game_center PRIVATE
    apps_common
    project_includes
)

# 设置输出目录
set_target_properties(game_center PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
