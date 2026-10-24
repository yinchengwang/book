# parser/sql 子模块：canonical SQL 解析器（Bison 语法 + 预生成文件）
#
# T6 (WS-C)：canonical 解析器收敛。generated/ 下的 gram.c/gram.h/scan.c
# 由 WSL2 的 flex/bison 预生成并随仓库提交，Windows 构建无需安装 flex/bison。
# 维护时以 -DREGENERATE_PARSER=ON 且安装 flex/bison 后重新生成。
#
# T4：简化版（手写解析器）已归档至 archive/db-sql-over-kv/，
# db_parser_sql_simplified 目标一并移除；makefuncs/parse_node/parse_analyze/
# parse_expr 为 canonical parser 依赖，编入 db_parser_sql。

option(REGENERATE_PARSER "Regenerate gram.c/scan.c from gram.y/scan.l (requires flex/bison)" OFF)

if(REGENERATE_PARSER)
    find_program(FLEX_EXECUTABLE flex REQUIRED)
    find_program(BISON_EXECUTABLE bison REQUIRED)
    set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY ${GEN_DIR})
    add_custom_command(
        OUTPUT ${GEN_DIR}/gram.c ${GEN_DIR}/gram.h
        COMMAND ${BISON_EXECUTABLE} --defines=${GEN_DIR}/gram.h --output=${GEN_DIR}/gram.c
                ${CMAKE_CURRENT_SOURCE_DIR}/gram.y
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gram.y
        COMMENT "Bison: Generating SQL parser from gram.y"
        VERBATIM)
    add_custom_command(
        OUTPUT ${GEN_DIR}/scan.c
        COMMAND ${FLEX_EXECUTABLE} -o ${GEN_DIR}/scan.c ${CMAKE_CURRENT_SOURCE_DIR}/scan.l
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scan.l
        COMMENT "Flex: Generating SQL lexer from scan.l"
        VERBATIM)
else()
    # 预生成文件（随仓库提交）
    set(GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
endif()

add_library(db_parser_sql STATIC
    ${GEN_DIR}/gram.c
    ${GEN_DIR}/scan.c
    sql_parse.c
    makefuncs.c
    parse_node.c
    parse_analyze.c
    parse_expr.c
)
target_include_directories(db_parser_sql PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${GEN_DIR}          # gram.h（token 定义）对 scan.c/调用者可见
)
target_link_libraries(db_parser_sql PRIVATE project_includes)
