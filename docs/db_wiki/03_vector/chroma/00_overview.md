# Chroma 项目概览

## 学习目标

- 了解 Chroma 的嵌入式向量数据库定位
- 掌握 Chroma "AI 原生"设计的核心思想

## 项目定位

> Chroma 是一个 AI 原生的嵌入式向量数据库，专为 LLM 应用和嵌入工作流设计，强调极简部署和开发体验。

**基本信息**：

- 开发方：Chroma 开源社区
- 首次发布：2022 年
- 开源协议：Apache 2.0
- GitHub Stars：约 18k（[chroma-core/chroma](https://github.com/chroma-core/chroma)）

## 核心设计理念

嵌入式零服务器、Python 原生 API、自动向量化、与 LLM 框架深度集成。

```mermaid
graph TD
    A[Chroma] --> B[嵌入式<br/>pip install 即刻使用]
    A --> C[Python 原生<br/>几行代码完成搜索]
    A --> D[自动向量化<br/>内置 Embedding 模型]
    A --> E[LLM 生态<br/>LangChain/LlamaIndex]

    B --> F[进程内运行<br/>无需 Docker/服务器]
    C --> G[List/Dict 风格 API]
    D --> H[sentence-transformers<br/>或其他模型]
```

## 对比

```mermaid
graph LR
    A[Chroma] --> B[嵌入式 最简单]
    A --> C[Python 原生]
    A --> D[适合学习/原型]

    E[Milvus] --> F[分布式 规模大]
    E --> G[生产级]

    H[Qdrant] --> I[Rust 高性能]
    H --> J[过滤能力强]
```

## 学习路线图

```mermaid
graph TD
    00[00 项目概览] --> 01[01 架构设计]
    01 --> 06[06 关键特性]
    06 --> 08[08 动手实验]
    01 --> 10[10 项目关联]
```

## 要点总结

- 嵌入式设计，零安装成本，最适合学习和原型
- Python 原生 API，几行代码即可运行
- 自动处理向量化，无需外部模型
- 与 LangChain/LlamaIndex 深度集成

## 思考题

1. Chroma 的"嵌入式"设计和传统数据库的"客户端-服务器"模式有什么区别？
2. 自动向量化功能在什么时候会成为瓶颈？
3. Chroma 适合生产环境吗？为什么？