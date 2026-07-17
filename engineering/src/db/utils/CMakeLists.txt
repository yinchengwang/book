# 数据库内核工具库
# 当前无源文件，创建空占位库以避免下游链接失败

# 没有 .c 源文件时，创建空库作为占位
add_library(db_utils STATIC ${CMAKE_CURRENT_SOURCE_DIR}/stub.c)
target_include_directories(db_utils PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/include/db/utils
)
