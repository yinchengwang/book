# 多模态嵌入式 SDK 核心库
cmake_minimum_required(VERSION 3.20)
project(mmsdk C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依赖：SQLite
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../third_part/sqlite3
                 ${CMAKE_BINARY_DIR}/sdk_sqlite3_build)

# 核心模块
set(SDK_CORE_SOURCES
    core/error.c
    core/sqlite_backend.c
    core/mmdb.c
    core/schema.c
    core/collection.c
    core/filter_parser.c
    core/result.c
)

# 向量模块
set(SDK_VECTORS_SOURCES
    vectors/vectors.c
    vectors/index_flat.c
    vectors/vectors_sql.c
)

# 图模块
set(SDK_GRAPH_SOURCES
    graph/graph.c
    graph/graph_traverse.c
    graph/graph_sql.c
)

# 时序模块
set(SDK_TIMESERIES_SOURCES
    timeseries/timeseries.c
    timeseries/agg.c
    timeseries/timeseries_sql.c
)

# 文本模块
set(SDK_TEXT_SOURCES
    text/text.c
    text/text_fts5.c
    text/text_sql.c
)

# 跨模型模块（P2 占位 + P3 混合检索）
set(SDK_EXTRA_SOURCES
    extra/cross_model.c
    extra/rag.c
    extra/rrf.c   # P3-T1.1 新增：RRF 多通道融合
    extra/hybrid_search.c   # P3-T1.2 新增：hybrid search 公共 API（向量+FTS5+RRF）
    extra/embedding_local.c   # P3-T2.1 新增：embedding 接口抽象 + hash/average-pool/openai 占位
    extra/embedding_openai.c   # P3-T2.2 新增：OpenAI adapter 占位
)

# 创建静态库
add_library(mmsdk STATIC
    ${SDK_CORE_SOURCES}
    ${SDK_VECTORS_SOURCES}
    ${SDK_GRAPH_SOURCES}
    ${SDK_TIMESERIES_SOURCES}
    ${SDK_TEXT_SOURCES}
    ${SDK_EXTRA_SOURCES}
)

# 公共包含路径（必须在 add_library 之后）
target_include_directories(mmsdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # Phase 2: faiss_hnsw.h 等 db 模块头文件
)

target_link_libraries(mmsdk PUBLIC sqlite3 faiss_hnsw)

# 编译选项
if(MSVC)
    target_compile_options(mmsdk PRIVATE /W4)
else()
    target_compile_options(mmsdk PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# 安装规则
install(TARGETS mmsdk
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../include/sdk/
    DESTINATION include/sdk
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)