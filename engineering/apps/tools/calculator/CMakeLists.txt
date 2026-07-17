# 创建可执行文件
file(GLOB CALCULATOR_SOURCES CONFIGURE_DEPENDS "*.c")
add_executable(calculator ${CALCULATOR_SOURCES})

# 链接公共头文件
target_link_libraries(calculator PRIVATE project_includes)