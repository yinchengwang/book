# SQLite amalgamation（来自 reference/relational/sqlite3 子模块）
# 提供 sqlite3 静态库 + 头文件路径

cmake_minimum_required(VERSION 3.20)
project(sqlite3 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 源文件：从 reference/relational/sqlite3/src 复制 amalgamation
# 若 third_part/sqlite3/sqlite3.c 存在则直接使用，否则跳过构建
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.c")
    add_library(sqlite3 STATIC sqlite3.c)
    target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # 禁用警告以保持编译干净
    if(MSVC)
        target_compile_options(sqlite3 PRIVATE /W0)
    else()
        target_compile_options(sqlite3 PRIVATE -w)
    endif()
else()
    # 占位：从系统查找 SQLite
    find_package(SQLite3)
    if(SQLite3_FOUND)
        add_library(sqlite3 ALIAS SQLite::SQLite3)
    else()
        message(FATAL_ERROR "SQLite3 not found. Place sqlite3.c in third_part/sqlite3/ or install system SQLite.")
    endif()
endif()
