# 包含头文件目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 自动获取 queue 目录下的所有 .c 文件
# file(GLOB QUEUE_SOURCES CONFIGURE_DEPENDS "*.c")

# 添加 queue 目录下的所有源文件为一个静态库
# add_library(queue STATIC ${QUEUE_SOURCES})