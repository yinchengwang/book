# C SDK 层
# 提供数据库统一 C API（SQL + 向量操作）

add_library(db_sdk STATIC
    src/db_sdk.c
)

target_include_directories(db_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(db_sdk PUBLIC
    sql_engine
    db_parser_sql_simplified
    db_storage
    db_core
    vector_api
    project_includes
)

# Windows SDK 也需要 ws2_32（通过 db_storage 传递）
if(WIN32)
    target_link_libraries(db_sdk PUBLIC ws2_32)
endif()
