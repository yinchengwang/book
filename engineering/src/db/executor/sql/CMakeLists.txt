# executor/sql 子模块：SQL 执行器
file(GLOB_RECURSE SQL_EXEC_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

if(SQL_EXEC_SOURCE_FILES)
    # 源文件会被父级 CMakeLists.txt 收集
endif()
