# 包含头文件目录
include_directories(${ENGINEERING_SOURCE_DIR}/include)

# 自动获取 redis 目录下的所有 .c 文件
file(GLOB REDIS_SOURCES CONFIGURE_DEPENDS "*.c")

# 添加 redis
add_library(redis STATIC ${REDIS_SOURCES})
message(STATUS "REDIS_SOURCES: ${REDIS_SOURCES}")