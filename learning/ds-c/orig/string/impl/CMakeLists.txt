# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 自动获取 str 目录下的所有 .c 文件
# file(GLOB STR_SOURCES CONFIGURE_DEPENDS "*.c")

# 添加 str
# add_library(str STATIC ${STR_SOURCES})
# message(STATUS "STR_SOURCES: ${STR_SOURCES}")

# 指定 str 库依赖于 queue 库
# target_link_libraries(str queue)