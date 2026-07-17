# RAG 系统工程化设计

## 概述

本文档定义 RAG 系统的工程化设计，包括 CMake 构建配置、依赖管理、测试策略和项目结构。

---

## 1. 项目结构

```
rag/
├── CMakeLists.txt                 # 根 CMake 配置
├── cmake/
│   ├── ProjectUtils.cmake        # CMake 工具函数
│   └── FindXXX.cmake            # 第三方库查找脚本
├── configs/
│   └── rag-config.yaml          # 默认配置文件
├── rag_data/                     # 数据目录 (gitignore)
│   ├── index/
│   ├── documents/
│   ├── cache/
│   └── logs/
├── models/                       # 模型目录 (gitignore)
│   ├── bge-base-zh-v1.5/
│   └── Qwen2.5-7B-Q4_K_M.gguf
├── docs/                         # 设计文档
│   ├── 01-data-layer.md
│   ├── 02-api-design.md
│   ├── 03-config-design.md
│   ├── 04-model-service.md
│   ├── 05-chunking-strategy.md
│   ├── 06-retrieval-strategy.md
│   ├── 07-prompt-engineering.md
│   ├── 08-observability.md
│   ├── 09-error-handling.md
│   ├── 10-engineering.md         # 本文档
│   └── diagrams/                 # UML 图
├── diagrams/                     # Excalidraw 图
│   └── *.excalidraw.json
├── include/
│   └── rag/
│       ├── rag.h                # 主头文件
│       ├── config.h
│       ├── engine.h
│       ├── parser.h
│       ├── chunker.h
│       ├── embed_service.h
│       ├── llm_service.h
│       ├── retriever.h
│       ├── vector_store.h
│       ├── hnsw_index.h
│       ├── bm25_index.h
│       ├── logger.h
│       ├── metrics.h
│       └── error.h
├── src/
│   ├── CMakeLists.txt
│   ├── main.cpp                 # CLI 入口
│   ├── server.cpp               # REST API 入口
│   ├── engine.cpp
│   ├── config.cpp
│   ├── parser/
│   │   ├── parser_registry.cpp
│   │   ├── markdown_parser.cpp
│   │   ├── pdf_parser.cpp
│   │   ├── code_parser.cpp
│   │   └── docx_parser.cpp
│   ├── chunker/
│   │   ├── chunker.cpp
│   │   ├── fixed_chunker.cpp
│   │   ├── recursive_chunker.cpp
│   │   ├── semantic_chunker.cpp
│   │   └── code_chunker.cpp
│   ├── embedding/
│   │   ├── embed_service.cpp
│   │   └── batch_queue.cpp
│   ├── llm/
│   │   ├── llm_service.cpp
│   │   └── llama_engine.cpp
│   ├── retrieval/
│   │   ├── retriever.cpp
│   │   ├── hnsw_retriever.cpp
│   │   ├── bm25_retriever.cpp
│   │   └── hybrid_retriever.cpp
│   ├── index/
│   │   ├── hnsw_index.cpp
│   │   └── bm25_index.cpp
│   ├── observability/
│   │   ├── logger.cpp
│   │   ├── metrics.cpp
│   │   └── health_check.cpp
│   └── error/
│       ├── error.cpp
│       └── error_handler.cpp
├── test/
│   ├── CMakeLists.txt
│   ├── test_main.cpp
│   ├── unit/
│   │   ├── test_config.cpp
│   │   ├── test_chunker.cpp
│   │   ├── test_parser.cpp
│   │   └── test_retriever.cpp
│   └── integration/
│       ├── test_index_build.cpp
│       └── test_query.cpp
├── third_party/
│   ├── llama.cpp/               # Git 子模块
│   ├── sentence-transformers/    # Git 子模块
│   └── hnswlib/                 # Git 子模块
├── scripts/
│   ├── build.sh                 # 构建脚本
│   ├── download_models.sh       # 下载模型脚本
│   └── test.sh                  # 测试脚本
└── README.md
```

---

## 2. CMake 构建配置

### 2.1 根 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(RAG VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# CMake 工具
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# 选项
option(RAG_BUILD_CLI "Build CLI executable" ON)
option(RAG_BUILD_SERVER "Build REST API server" ON)
option(RAG_BUILD_TESTS "Build tests" ON)
option(RAG_USE_CUDA "Enable CUDA support" OFF)

# 依赖
find_package(Threads REQUIRED)
find_package(OpenMP)

# 子项目
add_subdirectory(src)
if(RAG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
```

### 2.2 源代码 CMakeLists.txt

```cmake
# src/CMakeLists.txt

# ========== 核心库 ==========
add_library(rag_core STATIC
    rag/engine.cpp
    rag/config.cpp
    rag/error/error.cpp
    rag/error/error_handler.cpp
    rag/observability/logger.cpp
    rag/observability/metrics.cpp
    rag/observability/health_check.cpp
)
target_include_directories(rag_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(rag_core PRIVATE
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# ========== 解析器库 ==========
add_library(rag_parser STATIC
    rag/parser/parser_registry.cpp
    rag/parser/markdown_parser.cpp
    rag/parser/pdf_parser.cpp
    rag/parser/code_parser.cpp
)
target_link_libraries(rag_parser PRIVATE rag_core)

# ========== 分块器库 ==========
add_library(rag_chunker STATIC
    rag/chunker/chunker.cpp
    rag/chunker/fixed_chunker.cpp
    rag/chunker/recursive_chunker.cpp
    rag/chunker/semantic_chunker.cpp
    rag/chunker/code_chunker.cpp
)
target_link_libraries(rag_chunker PRIVATE rag_core rag_parser)

# ========== Embedding 库 ==========
add_library(rag_embedding STATIC
    rag/embedding/embed_service.cpp
    rag/embedding/batch_queue.cpp
)
target_link_libraries(rag_embedding PRIVATE
    rag_core
    sentence_transformers  # 或使用动态加载
)

# ========== LLM 库 ==========
add_library(rag_llm STATIC
    rag/llm/llm_service.cpp
    rag/llm/llama_engine.cpp
)
target_link_libraries(rag_llm PRIVATE
    rag_core
    llama.cpp
)

# ========== 检索库 ==========
add_library(rag_retrieval STATIC
    rag/retrieval/retriever.cpp
    rag/retrieval/hnsw_retriever.cpp
    rag/retrieval/bm25_retriever.cpp
    rag/retrieval/hybrid_retriever.cpp
    rag/index/hnsw_index.cpp
    rag/index/bm25_index.cpp
)
target_link_libraries(rag_retrieval PRIVATE
    rag_core
    rag_embedding
    hnswlib
    simsimd  # 高性能距离计算
)

# ========== 主库 ==========
add_library(rag STATIC
    IMPORTED
)
target_sources(rag PUBLIC
    FILE_SET HEADERS
    FILES
        ${CMAKE_SOURCE_DIR}/include/rag/rag.h
)
target_link_libraries(rag PUBLIC
    rag_core
    rag_parser
    rag_chunker
    rag_embedding
    rag_llm
    rag_retrieval
)

# ========== CLI 可执行文件 ==========
if(RAG_BUILD_CLI)
    add_executable(rag_cli
        rag/main.cpp
    )
    target_link_libraries(rag_cli PRIVATE
        rag
        Threads::Threads
    )
endif()

# ========== REST API 服务器 ==========
if(RAG_BUILD_SERVER)
    find_package(cpr CONFIG REQUIRED)
    find_package(httplib CONFIG REQUIRED)

    add_executable(rag_server
        rag/server.cpp
    )
    target_link_libraries(rag_server PRIVATE
        rag
        cpr::cpr
        httplib::httplib
        Threads::Threads
    )
endif()
```

### 2.3 测试 CMakeLists.txt

```cmake
# test/CMakeLists.txt

# GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 单元测试
add_executable(test_unit
    test_main.cpp
    unit/test_config.cpp
    unit/test_chunker.cpp
    unit/test_parser.cpp
    unit/test_retriever.cpp
)
target_link_libraries(test_unit PRIVATE
    rag
    GTest::gtest_main
    GTest::gmock_main
)
include(GoogleTest)

# 集成测试
add_executable(test_integration
    test_main.cpp
    integration/test_index_build.cpp
    integration/test_query.cpp
)
target_link_libraries(test_integration PRIVATE
    rag
    GTest::gtest_main
)

# 测试数据
file(COPY ${CMAKE_SOURCE_DIR}/test/data
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)
```

---

## 3. 依赖管理

### 3.1 外部依赖

| 依赖 | 版本 | 用途 | 加载方式 |
|------|------|------|----------|
| nlohmann-json | 3.10+ | JSON 解析 | CMake FetchContent |
| sqlite3 | 3.40+ | 元数据存储 | 系统库 |
| sentence-transformers | - | Embedding | Git 子模块 |
| llama.cpp | - | LLM 推理 | Git 子模块 |
| hnswlib | - | 向量索引 | Git 子模块 |
| cpr | 1.10+ | HTTP 客户端 | CMake FetchContent |
| httplib | 0.7+ | HTTP 服务器 | CMake FetchContent |
| spdlog | 1.11+ | 日志 | CMake FetchContent |
| simsimd | - | SIMD 距离 | Git 子模块 |

### 3.2 CMake FetchContent 配置

```cmake
# cmake/Dependencies.cmake

include(FetchContent)

# nlohmann-json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)
set(SPDLOG_FMT_EXTERNAL OFF)
FetchContent_MakeAvailable(spdlog)

# cpr
FetchContent_Declare(
    cpr
    GIT_REPOSITORY https://github.com/libcpr/cpr.git
    GIT_TAG 1.10.5
)
set(CPR_BUILD_TESTS OFF)
FetchContent_MakeAvailable(cpr)

# httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)
```

### 3.3 Git 子模块配置

```bash
# .gitmodules
[submodule "third_party/llama.cpp"]
    path = third_party/llama.cpp
    url = https://github.com/ggerganov/llama.cpp.git
[submodule "third_party/hnswlib"]
    path = third_party/hnswlib
    url = https://github.com/nmslib/hnswlib.git
[submodule "third_party/sentence-transformers"]
    path = third_party/sentence-transformers
    url = https://github.com/UKPLab/sentence-transformers.git
[submodule "third_party/simsimd"]
    path = third_party/simsimd
    url = https://github.com/ashzoq/simsimd.git
```

---

## 4. 测试策略

### 4.1 测试分层

```
┌─────────────────────────────────────────────────────────────┐
│                     测试金字塔                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                         ▲                                   │
│                        /│\                                 │
│                       / │ \                                │
│                      /  │  \          E2E 测试              │
│                     /   │   \        (少量核心场景)         │
│                    /────│────\                             │
│                   /     │     \                            │
│                  /      │      \      集成测试              │
│                 /       │       \     (按模块)              │
│                /────────│────────\                          │
│               /         │         \                         │
│              /          │          \    单元测试             │
│             /           │           \   (每个类/函数)         │
│            /____________│____________\                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 单元测试

```cpp
// test/unit/test_chunker.cpp

#include <gtest/gtest.h>
#include "rag/chunker/chunker.h"
#include "rag/chunker/recursive_chunker.h"

class RecursiveChunkerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ChunkConfig config;
        config.chunk_size = 100;
        config.chunk_overlap = 20;
        config.min_chunk_size = 20;
        chunker_ = std::make_unique<RecursiveChunker>(config);
    }

    std::unique_ptr<RecursiveChunker> chunker_;
};

// 测试基本分块
TEST_F(RecursiveChunkerTest, BasicChunking) {
    std::string text = "这是第一段文本。这是一个第二段。";
    auto chunks = chunker_->chunk(text, "doc_001");

    EXPECT_GT(chunks.size(), 0);
    EXPECT_FALSE(chunks[0].content.empty());
}

// 测试边界情况
TEST_F(RecursiveChunkerTest, EdgeCases) {
    // 空文本
    auto chunks1 = chunker_->chunk("", "doc_001");
    EXPECT_EQ(chunks1.size(), 0);

    // 短文本
    auto chunks2 = chunker_->chunk("短文本", "doc_002");
    EXPECT_EQ(chunks2.size(), 1);
}

// 测试重叠
TEST_F(RecursiveChunkerTest, Overlap) {
    std::string text = "这是第一段文本。这是一个第二段。这是一个第三段。";
    auto chunks = chunker_->chunk(text, "doc_001");

    if (chunks.size() >= 2) {
        // 验证有重叠内容
        EXPECT_TRUE(chunks[1].content.find(chunks[0].content) != std::string::npos ||
                    overlaps(chunks[0].content, chunks[1].content));
    }
}
```

### 4.3 集成测试

```cpp
// test/integration/test_index_build.cpp

#include <gtest/gtest.h>
#include "rag/engine.h"
#include "rag/config.h"

class IndexBuildIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时目录
        temp_dir_ = std::tmpnam(nullptr);
        std::filesystem::create_directory(temp_dir_);

        // 创建测试文件
        create_test_files();
    }

    void TearDown() override {
        std::filesystem::remove_all(temp_dir_);
    }

    void create_test_files() {
        std::ofstream file(temp_dir_ + "/test.md");
        file << "# 测试文档\n\n这是测试内容。";
        file.close();
    }

    std::string temp_dir_;
};

TEST_F(IndexBuildIntegrationTest, BuildAndQuery) {
    // 1. 加载配置
    Config config;
    config.data_sources[0].path = temp_dir_;
    config.data_sources[0].patterns = {"*.md"};

    // 2. 创建引擎
    Engine engine(config);

    // 3. 构建索引
    auto result = engine.build_index();
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.documents_count, 0);

    // 4. 查询
    auto answer = engine.query("测试内容是什么？");
    EXPECT_FALSE(answer.empty());
}
```

### 4.4 性能基准测试

```cpp
// test/benchmark/benchmark_retrieval.cpp

#include <benchmark/benchmark.h>
#include "rag/retriever.h"

static void BM_HNSWSearch(benchmark::State& state) {
    // Setup
    auto retriever = create_hnsw_retriever();

    for (auto _ : state) {
        auto results = retriever.search(test_vector_, 10);
    }
}
BENCHMARK(BM_HNSWSearch);

static void BM_BatchEmbedding(benchmark::State& state) {
    auto service = create_embedding_service();

    std::vector<std::string> texts;
    for (int i = 0; i < state.range(0); ++i) {
        texts.push_back(generate_test_text());
    }

    for (auto _ : state) {
        service.encode_batch(texts);
    }
}
BENCHMARK(BM_BatchEmbedding)->Arg(32)->Arg(64)->Arg(128);
```

---

## 5. 构建脚本

### 5.1 完整构建脚本

```bash
#!/bin/bash
# scripts/build.sh

set -e

BUILD_TYPE=${1:-Release}
BUILD_DIR=build
PROJECT_ROOT=$(cd "$(dirname "$0")/.." && pwd)

echo "=== RAG 构建脚本 ==="
echo "构建类型: $BUILD_TYPE"
echo "项目目录: $PROJECT_ROOT"

cd "$PROJECT_ROOT"

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 配置 CMake
echo "配置 CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DRAG_BUILD_CLI=ON \
    -DRAG_BUILD_SERVER=ON \
    -DRAG_BUILD_TESTS=ON \
    -DRAG_USE_CUDA=OFF

# 编译
echo "编译..."
cmake --build . --parallel $(nproc)

# 运行测试
echo "运行测试..."
ctest --output-on-failure

echo "=== 构建完成 ==="
echo "CLI: $PROJECT_ROOT/bin/rag_cli"
echo "Server: $PROJECT_ROOT/bin/rag_server"
```

### 5.2 模型下载脚本

```bash
#!/bin/bash
# scripts/download_models.sh

set -e

MODELS_DIR="${1:-$PWD/models}"
mkdir -p "$MODELS_DIR"

echo "=== RAG 模型下载脚本 ==="
echo "模型目录: $MODELS_DIR"

# 下载 Embedding 模型 (bge-base-zh-v1.5)
echo "下载 Embedding 模型..."
if [ ! -d "$MODELS_DIR/bge-base-zh-v1.5" ]; then
    git lfs install
    git clone https://huggingface.co/baai/bge-base-zh-v1.5 "$MODELS_DIR/bge-base-zh-v1.5"
else
    echo "Embedding 模型已存在，跳过"
fi

# 下载 LLM 模型 (Qwen2.5-7B-Q4)
echo "下载 LLM 模型..."
if [ ! -f "$MODELS_DIR/Qwen2.5-7B-Q4_K_M.gguf" ]; then
    git lfs install
    git clone https://huggingface.co/Qwen/Qwen2.5-7B-Instruct-GGUF "$MODELS_DIR/Qwen2.5-7B-Instruct-GGUF"
    # 选择 Q4 量化版本
    mv "$MODELS_DIR/Qwen2.5-7B-Instruct-GGUF/qwen2.5-7b-instruct-q4_k_m.gguf" "$MODELS_DIR/"
    rm -rf "$MODELS_DIR/Qwen2.5-7B-Instruct-GGUF"
else
    echo "LLM 模型已存在，跳过"
fi

echo "=== 下载完成 ==="
ls -la "$MODELS_DIR"
```

---

## 6. 持续集成

### 6.1 GitHub Actions 配置

```yaml
# .github/workflows/ci.yml

name: CI

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v4
      with:
        submodules: recursive

    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake ninja-build libsqlite3-dev

    - name: Configure
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release

    - name: Build
      run: cmake --build build --parallel

    - name: Test
      run: |
        cd build
        ctest --output-on-failure

  benchmark:
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main'

    steps:
    - uses: actions/checkout@v4

    - name: Build with benchmarks
      run: |
        cmake -B build -DCMAKE_BUILD_TYPE=Release -DRAG_BUILD_BENCHMARKS=ON
        cmake --build build --parallel

    - name: Run benchmarks
      run: ./build/bin/benchmark_retrieval

    - name: Upload results
      uses: actions/upload-artifact@v4
      with:
        name: benchmark-results
        path: build/benchmark_results.json
```

---

## 7. 代码规范

### 7.1 格式化配置

```yaml
# .clang-format
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
NamespaceIndentation: None
PointerAlignment: Left
ReferenceAlignment: Left
IncludeBlocks: Regroup
```

### 7.2 编译警告

```cmake
# 启用严格警告
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=return-type")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=uninitialized")
endif()
```

---

## 8. 发布流程

### 8.1 版本号规则

```
主版本.次版本.修订号
│       │      └── 错误修复
│       └────────── 新功能 (向后兼容)
└────────────────── 重大变化 (不向后兼容)
```

### 8.2 发布检查清单

```markdown
## 发布前检查清单

- [ ] 所有测试通过
- [ ] 性能基准达标
- [ ] 文档更新完成
- [ ] 版本号已更新
- [ ] CHANGELOG.md 已更新
- [ ] Git tag 已创建
- [ ] 发布包已构建
- [ ] SHA256 校验已生成
```

---

*文档版本：1.0.0*
*最后更新：2026-07-06*
