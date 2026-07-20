# Chroma 架构设计

## 学习目标

- 理解 Chroma 的嵌入式架构设计
- 掌握 Chroma 的核心组件

## 架构总览

```mermaid
graph TB
    API[Python API] --> CORE[Core 核心]
    CORE --> CATALOG[Metadata Catalog<br/>SQLite 存储元数据]
    CORE --> INDEX[Vector Index<br/>HNSW 索引]
    CORE --> EMBED[Embedding Function<br/>插件式模型]

    EMBED --> DEF[默认: all-MiniLM-L6-v2]
    EMBED --> OPENAI[OpenAI Embedding]
    EMBED --> HUG[HuggingFace 模型]
    EMBED --> CUSTOM[自定义函数]

    CORE --> PERSIST[持久化<br/>可选: 磁盘/内存]
```

## 数据模型

```mermaid
graph LR
    COLL[Collection<br/>集合] --> SEG[Schematic<br/>自动推断结构]
    COLL --> DOCS[Documents<br/>文档列表]
    DOCS --> D1[Document<br/>id + text + metadata + embedding]
    DOCS --> D2[...]
```

- **Collection**：类似关系数据库的表
- **Document**：文本 + 元数据 + 向量
- **Embedding**：向量表示，自动或手动生成

## 要点总结

- 核心组件：SQLite 元数据 + HNSW 索引 + Embedding 函数
- 数据模型：Collection → Document（含自动向量化）
- 可选持久化：内存（默认）或磁盘
- 嵌入式架构：进程内运行，无独立服务