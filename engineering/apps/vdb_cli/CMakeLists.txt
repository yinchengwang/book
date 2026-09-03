# MiniVecDB CLI 工具
# 向量数据库命令行界面

# 源文件
file(GLOB VDB_CLI_SOURCES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "*.c"
)

# 创建可执行文件
add_executable(vdb_cli ${VDB_CLI_SOURCES})

# 包含目录
target_include_directories(vdb_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
    ${ENGINEERING_SOURCE_DIR}/../third_part
)

# 链接库
target_link_libraries(vdb_cli PRIVATE
    vector_api
    db_core
    db_bm25
    project_includes
)
