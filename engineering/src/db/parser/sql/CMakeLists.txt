# parser/sql 子模块：SQL 解析器
#
# 包含两个版本：
# 1. 简化版（sql_lexer.c, sql_parser.c）- 手写解析器
# 2. 完整版（Flex/Bison 生成）- 生产级解析器

# 简化版源文件
set(SQL_PARSER_SIMPLIFIED_SOURCES
    sql_lexer.c
    sql_parser.c
    makefuncs.c
    parse_node.c
    parse_analyze.c
    parse_expr.c
)

# 创建简化版解析器库
add_library(db_parser_sql_simplified STATIC ${SQL_PARSER_SIMPLIFIED_SOURCES})
target_include_directories(db_parser_sql_simplified PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
)
target_link_libraries(db_parser_sql_simplified PRIVATE project_includes)

# 查找 Flex（可选）
find_program(FLEX_EXECUTABLE flex)
if(FLEX_EXECUTABLE)
    message(STATUS "Found Flex: ${FLEX_EXECUTABLE}")

    # Flex 词法分析器
    set(FLEX_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/scan.l")
    set(FLEX_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/lex.sql.c")

    add_custom_command(
        OUTPUT ${FLEX_OUTPUT}
        COMMAND ${FLEX_EXECUTABLE}
            -o ${FLEX_OUTPUT}
            ${FLEX_INPUT}
        DEPENDS ${FLEX_INPUT}
        COMMENT "Flex: Generating SQL lexer from scan.l"
        VERBATIM
    )
else()
    message(STATUS "Flex not found - Flex-based lexer will not be built")
    message(STATUS "Install Flex on Windows: choco install winflexbison3")
    message(STATUS "Install Flex on MSYS2: pacman -S flex")
endif()

# 查找 Bison（可选）
find_program(BISON_EXECUTABLE bison)
if(BISON_EXECUTABLE)
    message(STATUS "Found Bison: ${BISON_EXECUTABLE}")

    # Bison 语法分析器
    set(BISON_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/gram.y")
    set(BISON_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/gram.c")
    set(BISON_HEADER "${CMAKE_CURRENT_BINARY_DIR}/gram.h")

    # 使用 CMake 标准方法生成 Bison 输出
    add_custom_command(
        OUTPUT ${BISON_OUTPUT} ${BISON_HEADER}
        COMMAND ${BISON_EXECUTABLE}
            --defines=${BISON_HEADER}
            --output=${BISON_OUTPUT}
            ${BISON_INPUT}
        DEPENDS ${BISON_INPUT}
        COMMENT "Bison: Generating SQL parser from gram.y"
        VERBATIM
    )

    if(FLEX_EXECUTABLE)
        # 完整版解析器库（Flex + Bison + 节点构造 + 语义分析 + 表达式分析）
        set(SQL_PARSER_FULL_SOURCES
            ${FLEX_OUTPUT}
            ${BISON_OUTPUT}
            makefuncs.c
            parse_node.c
            parse_analyze.c
            parse_expr.c
        )

        add_library(db_parser_sql_full STATIC ${SQL_PARSER_FULL_SOURCES})
        target_include_directories(db_parser_sql_full PUBLIC
            ${ENGINEERING_SOURCE_DIR}/include
        )
        target_link_libraries(db_parser_sql_full PRIVATE project_includes)
    endif()
else()
    message(STATUS "Bison not found - Bison-based parser will not be built")
    message(STATUS "Install Bison on Windows: choco install winflexbison3")
    message(STATUS "Install Bison on MSYS2: pacman -S bison")
endif()

# 源文件会被父级 CMakeLists.txt 收集
# 这里仅创建局部库
