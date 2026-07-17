# 数据结构库 (Data Structures)

file(GLOB_RECURSE DS_SOURCES
    LIST_DIRECTORIES true
    "*/impl/*.c"
)

# 排除有兼容性问题的文件
list(FILTER DS_SOURCES EXCLUDE REGEX "circle_queue\.c$")

# 显式包含 tree.c（提供便利函数）
list(APPEND DS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tree/binary_tree/impl/tree.c
)

message(STATUS "DS_SOURCES: ${DS_SOURCES}")

add_library(ds STATIC ${DS_SOURCES})

target_include_directories(ds PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(ds m)

target_compile_options(ds PRIVATE
    -Wall -Wextra -Wpedantic -Wno-sign-compare
)
