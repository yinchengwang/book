# protocol 模块：协议兼容层 + SQL 方言翻译
# 提供 PostgreSQL/MySQL 线协议 和多方言 SQL 翻译

add_library(protocol OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/mysql_wire_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/analytics_sql.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pg_wire_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sql_compat.c
)

target_include_directories(protocol PRIVATE
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)

# 链接依赖
if(TARGET db_core)
    target_link_libraries(protocol PRIVATE db_core)
endif()
if(TARGET db_storage)
    target_link_libraries(protocol PRIVATE db_storage)
endif()
if(TARGET db_utils)
    target_link_libraries(protocol PRIVATE db_utils)
endif()

target_compile_options(protocol PRIVATE -Wno-unused-parameter)

# Windows 需要链接数学库
if(WIN32)
    target_link_libraries(protocol PRIVATE m)
endif()
