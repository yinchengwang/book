# 自动收集当前目录下所有 .cpp 源文件
file(GLOB SUDOKU_SOURCES CONFIGURE_DEPENDS "*.cpp")
add_executable(sudoku ${SUDOKU_SOURCES})

target_link_libraries(sudoku PRIVATE project_includes)

set_target_properties(sudoku PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
