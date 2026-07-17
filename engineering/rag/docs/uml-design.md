# RAG 系统设计文档

## 概述

本文档描述了为 C/C++ 算法与数据结构练习项目设计的生产级 RAG（检索增强生成）系统。系统支持索引项目中的 Markdown 文档、PDF、DOCX、Excel、LeetCode/Interview 代码等各类内容，并提供基于本地 Embedding 和 LLM 的问答能力。

---

## UML 图清单

| 序号 | 图类型 | 文件名 | 描述 |
|------|--------|--------|------|
| 1 | [用例图](diagrams/use-case.excalidraw.json) | use-case.excalidraw.json | 系统参与者与用例关系 |
| 2 | [类图](diagrams/class-diagram.excalidraw.json) | class-diagram.excalidraw.json | 核心类的结构与关系 |
| 3 | [对象图](diagrams/object-diagram.excalidraw.json) | object-diagram.excalidraw.json | 系统实例化展示 |
| 4 | [架构图](diagrams/architecture.excalidraw.json) | architecture.excalidraw.json | 三层架构设计 |
| 5 | [组件图](diagrams/component.excalidraw.json) | component.excalidraw.json | 组件依赖关系 |
| 6 | [部署图](diagrams/deployment.excalidraw.json) | deployment.excalidraw.json | 部署架构 |
| 7 | [数据流图](diagrams/data-flow.excalidraw.json) | data-flow.excalidraw.json | 数据处理流程 |
| 8 | [活动图](diagrams/activity-diagram.excalidraw.json) | activity-diagram.excalidraw.json | 业务流程活动 |
| 9 | [状态图](diagrams/state-diagram.excalidraw.json) | state-diagram.excalidraw.json | 索引生命周期状态 |
| 10 | [顺序图（入库）](diagrams/sequence-indexing.excalidraw.json) | sequence-indexing.excalidraw.json | 文档入库时序 |
| 11 | [顺序图（查询）](diagrams/sequence-query.excalidraw.json) | sequence-query.excalidraw.json | 问答查询时序 |
| 12 | [协作图](diagrams/collaboration-diagram.excalidraw.json) | collaboration-diagram.excalidraw.json | 交互对象协作 |
| 13 | [系统总览图](diagrams/system-overview.excalidraw.json) | system-overview.excalidraw.json | 系统整体概览 |

---

## 1. 用例图 (Use Case Diagram)

### 参与者

- **用户**：主要使用者，进行问答查询和文档管理
- **管理员**：负责系统配置和索引管理
- **定时任务**：自动触发索引重建

### 用例

| 用例 | 描述 |
|------|------|
| 问答查询 | 用户输入问题，系统返回基于检索的生成答案 |
| 索引构建 | 解析文档、分块、生成向量、存入索引库 |
| 文档管理 | 添加、删除、更新文档 |
| 系统配置 | 配置 Embedding 模型、LLM 参数、分块策略 |
| 定时重建索引 | 按计划自动全量重建索引 |
| 查询历史 | 查看历史问答记录 |
| 批量导入 | 批量导入多个文档 |
| 统计信息 | 查看索引统计信息 |

### 扩展关系

- 问答查询 → 查询历史（可选扩展）
- 文档管理 → 批量导入（可选扩展）
- 系统配置 → 统计信息（可选扩展）

---

## 2. 类图 (Class Diagram)

### 核心类

| 类名 | 类型 | 职责 |
|------|------|------|
| `DocumentParser` | 抽象类 | 文档解析基类 |
| `MarkdownParser` | 具体类 | 解析 Markdown 文档 |
| `PDFParser` | 具体类 | 解析 PDF 文档 |
| `CodeParser` | 具体类 | 解析代码文件 |
| `Chunker` | 抽象类 | 分块策略基类 |
| `SemanticChunker` | 具体类 | 语义分块 |
| `RecursiveCharacterChunker` | 具体类 | 递归字符分块 |
| `EmbeddingService` | 具体类 | 向量编码服务 |
| `VectorStore` | 具体类 | 向量存储（HNSW + BM25） |
| `HybridRetriever` | 具体类 | 混合检索器（RRF 融合） |
| `LLMService` | 具体类 | LLM 生成服务 |
| `RAGEngine` | 具体类 | RAG 引擎主类 |
| `Chunk` | 数据类 | 文档块 |

### 类关系

```
DocumentParser <|-- MarkdownParser
              <|-- PDFParser
              <|-- CodeParser
Chunker <|-- SemanticChunker
       <|-- RecursiveCharacterChunker

RAGEngine --> DocumentParser
RAGEngine --> Chunker
RAGEngine --> EmbeddingService
RAGEngine --> VectorStore
RAGEngine --> HybridRetriever
RAGEngine --> LLMService
EmbeddingService --> VectorStore
VectorStore --> HybridRetriever
HybridRetriever --> LLMService
```

---

## 3. 对象图 (Object Diagram)

对象图展示了系统各组件的实例化关系：

- `RAGEngine` 实例：主引擎
- `MarkdownParser` 实例：Markdown 解析器
- `SemanticChunker` 实例：语义分块器
- `EmbeddingService` 实例：`model="bge-base-zh-v1.5"`
- `VectorStore` 实例：`hnsw: HNSWIndex`
- `HybridRetriever` 实例：混合检索器
- `LLMService` 实例：`model="Qwen2.5-7B"`

### 实例数据

```cpp
chunk_001:Chunk {
    id: "chunk_001",
    content: "RAG系统...",
    metadata: {file: "a.md"},
    embedding: [0.12, -0.34, ...]
}
```

---

## 4. 架构图 (Architecture Diagram)

### 三层架构

```
┌─────────────────────────────────────────────────────┐
│                   表现层                              │
│   ┌─────────────┐         ┌─────────────┐            │
│   │    CLI      │         │  REST API   │            │
│   └─────────────┘         └─────────────┘            │
└─────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────┐
│                   业务逻辑层                          │
│   ┌─────────────┐ ┌─────────────┐ ┌─────────────┐    │
│   │ 索引构建器   │ │   检索器    │ │ LLM生成器   │    │
│   └─────────────┘ └─────────────┘ └─────────────┘    │
└─────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────┐
│                    数据层                             │
│   ┌─────────────┐ ┌─────────────┐ ┌─────────────┐    │
│   │  向量存储    │ │  文档存储   │ │  元数据存储  │    │
│   └─────────────┘ └─────────────┘ └─────────────┘    │
└─────────────────────────────────────────────────────┘
```

---

## 5. 组件图 (Component Diagram)

### 组件列表

| 组件 | 描述 |
|------|------|
| `CLI` | 命令行接口 |
| `REST API` | RESTful API 接口 |
| `IndexBuilder` | 索引构建组件 |
| `Retriever` | 检索组件 |
| `Parser` | 文档解析组件 |
| `Chunker` | 分块组件 |
| `Embedding` | 向量化组件 |
| `VectorStore` | 向量存储组件 |
| `LLMGenerator` | LLM 生成组件 |

### 依赖关系

```
CLI ──→ IndexBuilder
CLI ──→ Retriever
CLI ──→ Parser

REST API ──→ IndexBuilder
REST API ──→ Retriever

IndexBuilder ──→ Parser
IndexBuilder ──→ Chunker
IndexBuilder ──→ Embedding
IndexBuilder ──→ VectorStore

Retriever ──→ VectorStore
Retriever ──→ Embedding
Retriever ──→ LLMGenerator
```

---

## 6. 部署图 (Deployment Diagram)

### 节点

| 节点 | 类型 | 组件 |
|------|------|------|
| 本地开发环境 | 应用节点 | CLI 程序、REST API |
| 模型服务 | 服务节点 | Embedding 服务、LLM 服务 (Qwen) |
| 存储层 | 数据节点 | 向量存储 HNSW、文档存储文件系统、元数据 SQLite |

### 部署关系

```
[本地开发环境] ──访问──> [模型服务]
                        │
                        ├── Embedding 服务
                        └── LLM 服务 (Qwen)
[本地开发环境] ──读写──> [存储层]
                        ├── 向量存储 HNSW
                        ├── 文档存储文件系统
                        └── 元数据 SQLite
```

---

## 7. 数据流图 (Data Flow Diagram)

### 入库数据流

```
源文档 ──→ 解析器 ──→ 分块器 ──→ Embedding ──→ 向量存储
   │         │           │           │            │
   ▼         ▼           ▼           ▼            ▼
 MD/PDF    Text[]      Chunk[]    float[768]    Indexed
```

### 查询数据流

```
用户查询 ──→ 查询向量化 ──→ 检索器 ──→ 上下文 ──→ LLM生成 ──→ 答案
   │            │            │          │           │         │
   ▼            ▼            ▼          ▼           ▼         ▼
 Question    query_vec    chunks[]   Prompt    Response   Answer
```

### 数据类型

| 数据类型 | 格式 |
|----------|------|
| 源文档 | Markdown, PDF, DOCX, 代码 |
| 文本块 | `Chunk[]` |
| 向量 | `float[768]` |
| 生成答案 | `Answer` |

---

## 8. 活动图 (Activity Diagram)

### 业务流程

1. **开始** → 输入文档/问题
2. **判断输入类型**
   - 文档路径 → 解析文档 → 分块处理 → 生成向量 → 存入索引 → **结束**
   - 问题 → 向量化问题 → 混合检索 → LLM生成 → 返回结果 → **结束**

### 活动节点

| 活动 | 描述 |
|------|------|
| 解析文档 | 调用对应的 Parser 解析文档 |
| 分块处理 | 使用 Chunker 将文本分块 |
| 生成向量 | 调用 Embedding 服务生成向量 |
| 存入索引 | 将向量和元数据存入 VectorStore |
| 向量化问题 | 将用户问题转换为向量 |
| 混合检索 | HNSW + BM25 检索 + RRF 融合 |
| LLM生成 | 调用 LLM 服务生成答案 |
| 返回结果 | 返回最终答案给用户 |

---

## 9. 状态图 (State Machine Diagram)

### 索引生命周期状态

```
┌────────────┐   加载    ┌────────────┐   成功    ┌────────────┐
│  初始化状态  │ ──────> │   加载中   │ ──────> │   就绪状态  │
└────────────┘          └────────────┘          └────────────┘
      │                       │                       │
      │                       │ 失败                   │ 触发重建
      │                       ▼                       ▼
      │                 ┌────────────┐          ┌────────────┐
      └────────────────>│  错误状态  │ <────────│   重建中   │
                        └────────────┘          └────────────┘
                              │                       │
                              │ 重试恢复               │ 完成
                              ▼                       │
                        ┌────────────┐ ─────────────────┘
                        │   加载中   │
                        └────────────┘
```

### 状态说明

| 状态 | 描述 | 动作 |
|------|------|------|
| 初始化状态 | 无索引数据 | entry: 创建空索引 |
| 加载中 | 从磁盘加载索引 | - |
| 就绪状态 | 索引可用，可响应查询 | do: 可响应查询 |
| 构建中 | 正在解析、分块、编码 | - |
| 错误状态 | 构建/加载失败 | - |
| 重建中 | 全量重建索引 | - |

### 转换事件

| 事件 | 源状态 | 目标状态 |
|------|--------|----------|
| 加载 | 初始化状态 | 加载中 |
| 成功 | 加载中 | 就绪状态 |
| 失败 | 加载中 | 错误状态 |
| 触发重建 | 就绪状态 | 重建中 |
| 完成 | 重建中 | 就绪状态 |
| 失败 | 构建中 | 错误状态 |
| 重试恢复 | 错误状态 | 加载中 |
| 重置 | 错误状态 | 初始化状态 |

---

## 10. 顺序图 - 文档入库 (Sequence Diagram)

### 时序

```
用户           RAGEngine        Parser        Chunker       Embedding     VectorStore
 │                │               │              │              │              │
 │──build_index()>               │              │              │              │
 │                │──parse(doc)──>│              │              │              │
 │                │<──text[]──────│              │              │              │
 │                │               │──chunk(text)─>              │              │
 │                │               │<──chunks[]───│              │              │
 │                │               │              │──encode()───>│              │
 │                │               │              │<──vectors[]──│              │
 │                │               │              │              │──add()─────>│
 │                │               │              │              │<─success────│
 │<──success──────│               │              │              │              │
```

### 循环

`loop: 所有文档` - 对每个文档重复执行解析、分块、编码、存储流程

---

## 11. 顺序图 - 问答查询 (Sequence Diagram)

### 时序

```
用户           RAGEngine        Embedding      Retriever       LLMService
 │                │               │               │                │
 │──query(q)─────>│               │               │                │
 │                │──encode()────>│               │                │
 │                │<──query_vec───│               │                │
 │                │──hybrid_search(query_vec)───>│                │
 │                │<──chunks[] (RRF融合)─────────│                │
 │                │──generate(prompt+chunks)─────────────────────>│
 │                │<──answer──────────────────────────────────────│
 │<──answer───────│               │               │                │
```

### 说明

- `query_vec`: 问题编码后的向量
- `chunks[]`: RRF 融合后的相关文档块
- `answer`: LLM 生成的答案

---

## 12. 协作图 (Collaboration Diagram)

### 交互对象

| 序号 | 对象 | 角色 |
|------|------|------|
| 1 | 用户 | 发起查询 |
| 2 | RAGEngine | 协调者 |
| 3 | Embedding | 向量编码 |
| 4 | VectorStore | 存储与检索 |
| 5 | LLMService | 答案生成 |

### 消息流

```
1: 用户 ─────────────────────────────────────────────────> RAGEngine
   1.1: query(q)

2: RAGEngine ─────────────────────────────────────────────> Embedding
   1.2: encode(query)

3: Embedding ─────────────────────────────────────────────> VectorStore
   1.3: query_vec

4: VectorStore ────────────────────────────────────────────> LLMService
   1.4: hybrid_search()

5: LLMService ─────────────────────────────────────────────> RAGEngine
   1.5: chunks[]

6: LLMService ─────────────────────────────────────────────> RAGEngine
   1.6: generate()

7: RAGEngine ─────────────────────────────────────────────> 用户
   1.7: answer
```

### 内部实现说明

```
┌─────────────────────────────────────────┐
│           VectorStore 内部              │
│                                         │
│   HNSW检索 ──┐                          │
│             ├──> RRF 融合 ──> chunks[]  │
│   BM25检索 ──┘                          │
└─────────────────────────────────────────┘
```

---

## 13. 系统总览图 (System Overview)

### 入库流程

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  文档源   │ ──> │   解析    │ ──> │   分块    │ ──> │ Embedding │ ──> │ 向量存储  │
└──────────┘     └──────────┘     └──────────┘     └──────────┘     └──────────┘
     │                                                            │
     ▼                                                            ▼
源文档文件                                                  已索引向量库
```

### 查询流程

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│   用户   │ ──> │  问答    │ ──> │  LLM生成  │ ──> │   返回    │ ──> │   用户   │
└──────────┘     └──────────┘     └──────────┘     └──────────┘     └──────────┘
```

---

## 文件结构

```
rag/
├── diagrams/
│   ├── activity-diagram.excalidraw.json    # 活动图
│   ├── architecture.excalidraw.json        # 架构图
│   ├── class-diagram.excalidraw.json       # 类图
│   ├── collaboration-diagram.excalidraw.json  # 协作图
│   ├── component.excalidraw.json           # 组件图
│   ├── data-flow.excalidraw.json            # 数据流图
│   ├── deployment.excalidraw.json          # 部署图
│   ├── object-diagram.excalidraw.json      # 对象图
│   ├── sequence-indexing.excalidraw.json   # 顺序图-入库
│   ├── sequence-query.excalidraw.json      # 顺序图-查询
│   ├── state-diagram.excalidraw.json        # 状态图
│   ├── system-overview.excalidraw.json      # 系统总览图
│   └── use-case.excalidraw.json             # 用例图
├── docs/
│   └── design.md                            # 设计文档
└── src/
    └── ...                                   # 源代码
```

---

## 使用说明

1. **查看图文件**：在 VSCode 中安装 Excalidraw 插件，直接打开 `.excalidraw.json` 文件即可查看和编辑
2. **编辑图**：使用 Excalidraw 插件的编辑功能，修改后保存即可
3. **导出**：可以将 Excalidraw 图导出为 PNG、SVG 等格式

---

*文档版本：1.0.0*  
*最后更新：2026-07-06*
