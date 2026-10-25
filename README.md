# D-code-book — C/C++ 算法工程实践

> 一个 C/C++ 工程实践项目：**`engine/`** 产出生产级作品。学习轨与参考轨已切出为独立仓。

[![CI](https://img.shields.io/badge/CI-passing-brightgreen)](.github/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)
[![Docs](https://img.shields.io/badge/docs-215%2B-brightgreen)](docs/)

---

## 🎯 项目定位

本项目是 **C/C++ 工程能力** 的系统性实践，覆盖：

| 领域 | 说明 |
|------|------|
| **数据库内核** | 存储引擎、SQL 执行器、事务处理 |
| **向量数据库** | 20+ ANN 索引实现、HNSW/IVF-PQ/DiskANN |
| **RAG 系统** | 检索增强生成、混合检索、Graph RAG |
| **分布式系统** | 分片、共识算法、分布式事务 |
| **算法工程** | 生产级排序、K-Means、量化压缩 |

---

## 🏗️ 核心能力

### 1. 存储引擎（PostgreSQL 风格）

- **Buffer Pool**：Clock-Sweep 置换、Hash 表 O(1) 查找、脏页管理
- **WAL 日志**：写前日志、Redo 恢复、检查点
- **Access Method**：Heap 堆表、B+Tree 索引、向量索引
- **Catalog 系统**：表/列/索引元数据、OID 分配

### 2. 向量索引（20+ ANN 实现）

| 类型 | 实现 |
|------|------|
| 图索引 | HNSW、NSW、SSG、ScaNN |
| 倒排索引 | IVF-Flat、IVF-PQ、IVF-HNSW |
| 树索引 | KD-Tree、Ball-Tree |
| 哈希索引 | LSH、ITQ、Spectral Hash |
| 量化 | PQ、SQ、OPQ、RQ、LVQ |
| 磁盘索引 | DiskANN |

### 3. RAG 检索增强生成

- **混合检索**：向量检索 + BM25 + RRF 融合
- **Graph RAG**：知识图谱 + 实体提取 + 子图检索
- **多模态支持**：Markdown、PDF、代码、图片
- **智能分块**：递归字符、语义、代码感知分块
- **RAG 评估**：RAGAs 指标体系

### 4. 分布式架构

- **分片策略**：Hash 分片、Range 分片、一致性 Hash
- **共识算法**：Raft Leader 选举、日志复制
- **分布式事务**：2PC 两阶段提交、SAGA 补偿
- **高可用**：故障检测、成员变更

---

## 📁 项目结构

```
D-code-book/
├── engine/              # 🎯 工程轨（生产级代码）
│   ├── src/
│   │   ├── db/             # 数据库存储引擎
│   │   ├── index/         # 20+ ANN 向量索引
│   │   ├── rag/           # RAG 系统
│   │   ├── graph/         # 图引擎
│   │   ├── algo-prod/     # 生产算法库
│   │   └── redis/         # Redis 核心移植
│   ├── rag/               # RAG 系统（含 Python SDK + Web UI）
│   └── apps/              # 独立应用
│
├── vendor/                # 📦 第三方依赖与共享 CMake 工具
│   ├── third_part/        # googletest / uthash / sqlite3 / cmocka
│   └── cmake/             # 共享 CMake 工具
│
├── docs/                  # 架构文档（arch/ storage/ analysis/ process/ diagrams/）
├── openspec/              # 变更管理
├── archive/               # 归档
└── scripts/               # 共享脚本
```

---

## 🚀 快速开始

### 构建工程轨

```bash
cmake -B build/engine -S engine -DBUILD_TESTING=ON
cmake --build build/engine --parallel 4
ctest --test-dir build/engine --output-on-failure
```

### 启动 RAG 服务

```bash
# 终端 1: 启动后端（端口 8080）
cd engine/rag
./build/rag_server

# 终端 2: 启动前端（端口 5173）
cd web
npm install && npm run dev
```

### RAG 快速使用

```bash
# 构建索引
./bin/rag_cli index build --path ./docs

# 问答
./bin/rag_cli query "HNSW 索引如何构建？"
```

---

## 🧪 测试覆盖

| 轨道 | 测试数 | 说明 |
|------|--------|------|
| 工程轨 | 104+ | 存储引擎、索引、SQL 执行器 |
| 学习轨 | 158+ | 算法题解、教学代码 |
| RAG | 20+ | 分块、检索、重排序 |

---

## 📚 文档导航

| 文档 | 内容 |
|------|------|
| [engine/rag/README.md](engine/rag/README.md) | RAG 系统架构与实现 |
| [engine/rag/docs/](engine/rag/docs/) | RAG 详细设计文档（10 篇） |
| [web/README.md](web/README.md) | Web 界面技术栈与组件 |
| [docs/](docs/) | 存储引擎、SQL 执行器架构文档 |
| [docs/arch/](docs/arch/) | 向量索引理论实现 |

---

## 📦 什么该放这个仓

本仓是四个仓中的**工程仓**（projects）。判断一份东西该放这里、还是放 `learning` / `ai-docs` / `reference`：

| 内容 | 归属 |
|---|---|
| 可运行的工程代码、测试、构建脚本 | projects |
| 项目级架构文档、流程文档 | projects |
| **项目级** agent 规约（`.ai/`、`CLAUDE.md`、`AGENTS.md`、`openspec/`、`skills-lock.json`） | projects |
| 项目专属数据（`dataset/`） | projects |
| 第三方源码镜像 | reference |
| **跨项目**通用笔记、模板、方法论文档 | ai-docs |
| 学习笔记、课程进度、算法题解 | learning |
| 学习轨的 spec（`openspec/specs/learning/`） | learning（待迁） |

**判定口诀**：换个项目还用得上 → ai-docs；只对这个项目有意义 → 留在本仓。

> ⚠️ `.ai/`、`CLAUDE.md`、`AGENTS.md` 是项目级 agent 规约，**禁止搬走** —— 搬走会导致 agent 读不到规约。
>
> 完整目录规范见父仓 `StillEvolving/docs/architecture.md`；机器可读版见父仓 `repos.yaml` 的 `must_contain` 字段。

---

## 📄 许可

MIT License - 详见 [LICENSE](LICENSE)
