# multimodal_rag server app

cmake_minimum_required(VERSION 3.20)
project(multimodal_rag LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# cpp-httplib: header-only library, only need httplib.h for include
set(HTTPLIB_HEADER "${ENGINEERING_SOURCE_DIR}/third_part/cpp-httplib/httplib.h")
if(NOT EXISTS "${HTTPLIB_HEADER}")
    message(FATAL_ERROR
        "cpp-httplib header not found at ${HTTPLIB_HEADER}. "
        "Adjust HTTPLIB_HEADER in apps/multimodal_rag/CMakeLists.txt.")
endif()

# httplib is header-only, no need to compile httplib.cpp
# Just create an INTERFACE library for include paths
add_library(cpp_httplib INTERFACE)
target_include_directories(cpp_httplib INTERFACE
    ${ENGINEERING_SOURCE_DIR}/third_part/cpp-httplib
)
# Note: Do NOT define CPPHTTPLIB_OPENSSL_SUPPORT at all (even =0).
# httplib.h uses #ifdef (not #if) to gate OpenSSL code; defining it to 0
# still triggers the OpenSSL version check which fails without OpenSSL headers.
# Simply omitting the macro disables all OpenSSL support cleanly.
#
# IMPORTANT (Linux portability): httplib.h's own #ifdef _WIN32_WINNT check
# will activate Windows socket code paths if this macro is set. On Linux,
# <winsock2.h> does not exist, so unconditionally defining _WIN32_WINNT
# breaks compilation on Linux. Gate it under if(WIN32).
target_compile_definitions(cpp_httplib INTERFACE
    RAG_USE_HTTPLIB=1
)
if(WIN32)
    target_compile_definitions(cpp_httplib INTERFACE
        _WIN32_WINNT=0x0A00
        WINVER=0x0A00
    )
endif()

# Phase 2: migrated mmrag sources. We use **per-subdirectory GLOBs** instead of
# a global recursive glob + blacklist. This means:
#   - Adding a new file to an *already-globbed* subdirectory compiles automatically.
#   - To add a brand-new subdirectory, just append one `file(GLOB ...)` line below.
#   - The "blacklist" only needs to exclude *specific* orphan files inside a
#     partially-globbed subdirectory (e.g., pipeline/, retrieval/) — not the
#     whole subdirectory.
#
# main.cpp is listed explicitly (top-level file) and added separately.
file(GLOB MMRAG_SOURCES_CORE CONFIGURE_DEPENDS src/core/*.cpp)
file(GLOB MMRAG_SOURCES_EMBED CONFIGURE_DEPENDS src/embed/*.cpp)
file(GLOB MMRAG_SOURCES_ENGINE CONFIGURE_DEPENDS src/engine/*.cpp)
file(GLOB MMRAG_SOURCES_INDEX CONFIGURE_DEPENDS src/index/*.cpp src/index/*.c)
file(GLOB MMRAG_SOURCES_LLM CONFIGURE_DEPENDS src/llm/*.cpp)
file(GLOB MMRAG_SOURCES_OBSERVABILITY CONFIGURE_DEPENDS src/observability/*.cpp)
file(GLOB MMRAG_SOURCES_PIPELINE CONFIGURE_DEPENDS src/pipeline/*.cpp)
file(GLOB MMRAG_SOURCES_RETRIEVAL CONFIGURE_DEPENDS src/retrieval/*.cpp)
file(GLOB MMRAG_SOURCES_SERVER CONFIGURE_DEPENDS src/server/*.cpp)
file(GLOB MMRAG_SOURCES_UTIL CONFIGURE_DEPENDS src/util/*.cpp)

# Subdirectories that are intentionally NOT globbed (entire subdir excluded):
#   - src/agent/        — Phase 7+ agent module, not yet wired in
#   - src/eval/         — depends on nlojo/json (available) but not wired in main
#   - src/graph/        — orphan, never wired into engine
# To enable any of these, add a `file(GLOB ...)` line above — no other edits
# required (new files will then be picked up automatically).

# Pipeline: glob the subdir, then exclude files not yet wired into engine.
# advanced_pipeline.cpp, naive_pipeline.cpp, iterative_pipeline.cpp, recursive_pipeline.cpp
# are now COMPILED (they have real implementations wired into the server).
# pipeline_base.cpp provides generate_with_llm() used by all pipelines — must compile
list(FILTER MMRAG_SOURCES_PIPELINE EXCLUDE REGEX ".*(corrective|graph|hybrid|hyde|react|self_rag|hyde_expander)_pipeline\.cpp$")

# Retrieval: glob the subdir, then exclude files that depend on excluded modules
# or have signature mismatches.
list(FILTER MMRAG_SOURCES_RETRIEVAL EXCLUDE REGEX ".*(bge_reranker|crossmodal_retriever|enhanced_retriever|kbase_retriever|lru_cache|metadata_filter|query_classifier|query_decomposer|query_expander|reranker|semantic_cache|synonym_expander|hyde_expander|retrieval_cache)\.cpp$")

# Server: api_server.cpp is the canonical entry point (82 KB raw-socket impl
# wired into main() via mmrag::api::create_server()). server.cpp is a 30-line
# httplib-based forwarder that targets the OLD mmrag::Server API and is dead code.
list(FILTER MMRAG_SOURCES_SERVER EXCLUDE REGEX ".*/server\.cpp$")

# Engine: incremental_indexer.cpp uses dirent.h (POSIX-only, breaks MSVC).
# graph_extension.cpp is a stub.
list(FILTER MMRAG_SOURCES_ENGINE EXCLUDE REGEX ".*incremental_indexer\.cpp$")
list(FILTER MMRAG_SOURCES_ENGINE EXCLUDE REGEX ".*graph_extension\.cpp$")

# Embed: embedding.cpp is the SimpleEmbeddingService random-hash implementation.
# Phase 3 replaces it with kbase; keep excluded until legacy callers are gone.
list(FILTER MMRAG_SOURCES_EMBED EXCLUDE REGEX ".*/embedding\.cpp$")

# LLM: llm_client.cpp / llm_service.cpp require libcurl. The HTTP backend
# (http_embedding.cpp / ollama_llm.cpp) was removed; only kbase stub + http
# embedding remain. Keep these excluded — no LLM service is provided.
list(FILTER MMRAG_SOURCES_LLM EXCLUDE REGEX ".*(llm_client|llm_service)\.cpp$")

# Core: schema_manager.cpp references SchemaManager / CURRENT_VERSION which
# are not yet declared in database.h (Phase 6 db_storage dependency).
list(FILTER MMRAG_SOURCES_CORE EXCLUDE REGEX ".*schema_manager\.cpp$")

set(MMRAG_SOURCES
    ${MMRAG_SOURCES_CORE}
    ${MMRAG_SOURCES_EMBED}
    ${MMRAG_SOURCES_ENGINE}
    ${MMRAG_SOURCES_INDEX}
    ${MMRAG_SOURCES_LLM}
    ${MMRAG_SOURCES_OBSERVABILITY}
    ${MMRAG_SOURCES_PIPELINE}
    ${MMRAG_SOURCES_RETRIEVAL}
    ${MMRAG_SOURCES_SERVER}
    ${MMRAG_SOURCES_UTIL}
)

# 自研 KV 存储引擎（替代 SQLite）
# db_storage 已在 target_link_libraries 中链接

# Add explicit dependencies for modular pipeline headers
# (Optional, helps IDEs but not strictly required for build)

add_executable(multimodal_rag_server
    src/main.cpp
    ${MMRAG_SOURCES}
)

target_include_directories(multimodal_rag_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # nlohmann/json, single-header (used by eval/, llm/, agent/)
    ${ENGINEERING_SOURCE_DIR}/third_part/nlohmann/single_include
    # Compatibility path for the few not-yet-migrated `rag/...` references
    # (engineering/include/rag/{multimodal_chunk,rag_err}.h live outside rag/).
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(multimodal_rag_server
    PRIVATE
        project_includes
        vector_index
        db_storage
        db_bm25
        apps_common
        cpp_httplib
        Threads::Threads
)

if(WIN32)
    target_link_libraries(multimodal_rag_server PRIVATE ws2_32 crypt32)
endif()

set_target_properties(multimodal_rag_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    FOLDER "Apps/multimodal_rag"
)

message(STATUS "Added multimodal_rag_server app")
