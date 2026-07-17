# RAG 子项目 CMakeLists.txt
# 独立项目，不依赖主项目的 FetchContent

cmake_minimum_required(VERSION 3.20)
project(RAG LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========== 输出配置 ==========
message(STATUS "=== RAG Build Configuration ===")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# ========== 依赖管理 ==========

# FetchContent 用于外部依赖
include(FetchContent)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ========== 查找依赖 ==========

# Threads
find_package(Threads REQUIRED)
message(STATUS "Threads: ${CMAKE_THREAD_LIBS_INIT}")

# SQLite3 - 使用 CMake 内置查找
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    message(STATUS "SQLite3: found (${SQLite3_INCLUDE_DIRS})")
    set(RAG_SQLITE3_LIBS SQLite3::SQLite3)
else()
    message(WARNING "SQLite3 not found, database functionality disabled")
    set(RAG_SQLITE3_LIBS "")
endif()

# ========== 子目录 ==========

add_subdirectory(src/rag/error)
add_subdirectory(src/rag/logger)
add_subdirectory(src/rag/config)
add_subdirectory(src/rag/data)
add_subdirectory(src/rag/chunker)
add_subdirectory(src/rag/parser)
add_subdirectory(src/rag/query_expansion)
add_subdirectory(src/rag/index)
add_subdirectory(src/rag/retrieval)
add_subdirectory(src/rag/embedding)
add_subdirectory(src/rag/reranker)
add_subdirectory(src/rag/knowledge_graph)
add_subdirectory(src/rag/entity_extraction)
add_subdirectory(src/rag/graph_retrieval)
add_subdirectory(src/rag/llm)
add_subdirectory(src/rag/evaluator)
add_subdirectory(src/rag/persist)
add_subdirectory(src/rag/metrics)
add_subdirectory(src/rag/engine)
add_subdirectory(src/rag/server)
add_subdirectory(src/rag/cli)

# ========== CLI 可执行文件 ==========

add_executable(rag_cli
    $<TARGET_OBJECTS:rag_cli_objects>
)
target_link_libraries(rag_cli PRIVATE
    rag_error
    rag_logger
    rag_config
    rag_data
    rag_parser
    rag_chunker
    rag_index
    rag_retrieval
    rag_embedding
    rag_reranker
    rag_knowledge_graph
    rag_entity_extraction
    rag_graph_retrieval
    rag_query_expansion
    rag_llm
    rag_evaluator
    rag_persist
    rag_engine
    rag_server
    Threads::Threads
    ${RAG_SQLITE3_LIBS}
    ws2_32
)
target_include_directories(rag_cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rag
)

# ========== 测试 ==========
option(RAG_BUILD_TESTS "Build tests" ON)
if(RAG_BUILD_TESTS)
    message(STATUS "Tests: enabled")
    # 测试需要 SQLite3
    if(SQLite3_FOUND)
        enable_testing()
        add_subdirectory(test/rag)
    else()
        message(WARNING "Tests require SQLite3, skipping test build")
    endif()
endif()

message(STATUS "=== RAG Build Configuration Complete ===")
