# RAG 编译指南

## 环境要求

- **CMake** 3.20+
- **C++17** 编译器 (MSVC, GCC, or Clang)
- **CUDA** 11.8+ (仅在使用 GPU 支持时需要)
- **8GB+ GPU 显存** (仅在使用 GPU 时需要)
- **SQLite3** (用于测试，可选)

## 快速开始

### Windows

```cmd
build.bat
```

或指定 Debug 构建:

```cmd
build.bat Debug
```

### Linux / macOS

```bash
chmod +x build.sh
./build.sh
```

## 项目结构

```
rag/
├── src/rag/              # 核心库
│   ├── cache/            # 语义缓存
│   ├── pipeline/         # 处理流水线
│   ├── async/            # 异步处理
│   ├── gpu/              # GPU 加速
│   ├── batch/            # 批处理
│   ├── chunker/          # 文档分块
│   ├── parser/           # 文档解析
│   ├── query_expansion/  # 查询扩展
│   ├── index/            # 索引结构
│   ├── retrieval/        # 检索器
│   ├── embedding/        # 向量化
│   ├── reranker/         # 重排序
│   ├── fusion/           # 融合检索
│   ├── knowledge_graph/  # 知识图谱
│   ├── entity_extraction/# 实体抽取
│   ├── graph_retrieval/  # 图检索
│   ├── llm/              # LLM 接口
│   ├── evaluator/        # 评估器
│   ├── persist/          # 持久化
│   ├── metrics/          # 指标
│   ├── engine/           # 引擎
│   ├── server/           # HTTP 服务
│   ├── cli/              # 命令行工具
│   ├── incremental/      # 增量索引
│   ├── fallback/         # 回退策略
│   ├── crossmodal/       # 跨模态检索
│   ├── expander/         # 查询扩展器
│   ├── selfrag/          # Self-RAG
│   ├── community/        # 社区检测
│   ├── multimodal/       # 多模态解析
│   ├── monitor/          # 监控
│   ├── error/            # 错误处理
│   ├── logger/           # 日志
│   ├── config/           # 配置
│   └── data/             # 数据结构
├── test/rag/             # 测试
└── include/rag/          # 头文件
```

## CMake 选项

| 选项 | 默认值 | 描述 |
|------|--------|------|
| `RAG_BUILD_TESTS` | `ON` | 是否编译测试 |

## 常见问题

### CUDA 未找到

如果 CMake 提示 CUDA 未找到，但你有 NVIDIA GPU:

1. 确保安装了 [CUDA Toolkit](https://developer.nvidia.com/cuda-downloads)
2. 确保 `PATH` 环境变量包含 CUDA bin 目录
3. 重新运行 CMake 配置

### SQLite3 未找到

测试需要 SQLite3。如果不需要测试，可以关闭:

```bash
cmake .. -DRAG_BUILD_TESTS=OFF
```

### 编译错误

如果遇到编译错误，确保:

1. 使用支持 C++17 的编译器
2. CMake 版本 >= 3.20
3. 所有子模块都已正确检出

## 模块说明

### 核心模块

- **cache** - LRU 和语义缓存
- **pipeline** - RAG 处理流水线
- **async** - 异步线程池
- **gpu** - CUDA GPU 加速

### 检索相关

- **chunker** - 文档分块 (固定大小/递归/Semantic)
- **parser** - 文档解析 (Markdown/PDF/Code)
- **embedding** - 向量化服务
- **index** - HNSW/BM25 索引
- **retrieval** - 检索器
- **reranker** - 重排序
- **fusion** - 融合检索 (RRF)

### LLM 相关

- **llm** - LLM 接口 (Ollama/llama.cpp)
- **query_expansion** - 查询扩展
- **evaluator** - 答案评估

### 高级特性

- **knowledge_graph** - 知识图谱
- **entity_extraction** - 实体抽取
- **graph_retrieval** - 图检索
- **selfrag** - Self-RAG
- **incremental** - 增量索引
- **fallback** - 回退策略
- **multimodal** - 多模态解析
- **crossmodal** - 跨模态检索
- **community** - 社区检测
- **monitor** - 监控
