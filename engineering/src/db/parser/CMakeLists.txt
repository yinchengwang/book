# parser 模块：语法解析层
# 依赖 core

# 子模块（如果存在 CMakeLists.txt，则添加子目录）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sql/CMakeLists.txt")
    add_subdirectory(sql)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/graph/CMakeLists.txt")
    add_subdirectory(graph)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/framework/CMakeLists.txt")
    add_subdirectory(framework)
endif()

# 收集父级源文件（排除子模块）
file(GLOB PARSER_SOURCE_FILES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.c")

# 创建父级库（如果子模块不存在）
if(NOT TARGET db_parser_sql_simplified AND NOT TARGET db_parser)
    if(PARSER_SOURCE_FILES)
        add_library(db_parser STATIC ${PARSER_SOURCE_FILES})
        target_include_directories(db_parser PUBLIC
            ${ENGINEERING_SOURCE_DIR}/include
            ${ENGINEERING_SOURCE_DIR}/include/db
            ${ENGINEERING_SOURCE_DIR}/include/db/parser
            ${ENGINEERING_SOURCE_DIR}/include/db/core
        )
        target_link_libraries(db_parser PRIVATE db_core)
        target_compile_options(db_parser PRIVATE -Wno-unused-parameter)
    endif()
endif()
