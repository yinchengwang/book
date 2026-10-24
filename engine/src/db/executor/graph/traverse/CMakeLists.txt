# executor/graph/traverse 子模块：图遍历
file(GLOB_RECURSE TRAVERSE_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(TRAVERSE_SOURCE_FILES)
    # 源文件会被父级 CMakeLists.txt 收集
endif()
