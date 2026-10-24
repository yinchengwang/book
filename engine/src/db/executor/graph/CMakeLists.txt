# executor/graph 子模块：Graph 执行器
file(GLOB_RECURSE GRAPH_EXEC_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(GRAPH_EXEC_SOURCE_FILES)
    # 源文件会被父级 CMakeLists.txt 收集
endif()
