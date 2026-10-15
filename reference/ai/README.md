# reference/ai/ 目录说明

本目录收录开源 RAG 与 Agent 框架源码，作为 AI 应用层学习参考。不参与工程构建。

## 子目录

- `rag/`：11 个开源 RAG 框架（详见 [rag/README.md](rag/README.md)）。
- `agent/`：13 个开源 Agent 框架（详见 [agent/README.md](agent/README.md)）。

## 用途

- 撰写 Wiki / 学习笔记时的源码对照
- 调研主流 AI 应用层框架的设计模式
- 复用算法参考工程已下载的开源项目（参见 `engineering/include/`）

## 约束

- 这些子目录都是独立的 Git 仓库（通过 `git submodule add` 或 `git clone --depth 1` 建立），不参与主仓库构建。
- 不要在主仓库直接 `git add` 其内部文件；如需更新，使用各自子目录的 `git fetch --depth 1 origin` + `git reset --hard origin/<branch>` 流程。
- 总磁盘占用约 1.5 GB；新增前先评估空间。
