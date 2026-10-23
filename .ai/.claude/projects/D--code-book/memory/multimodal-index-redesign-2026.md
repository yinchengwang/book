---
name: multimodal-index-redesign-2026
description: 多模态数据库索引子系统重构：分层架构、持久化开关、统一向量存储
metadata: 
  node_type: memory
  type: project
  originSessionId: b8744f8b-f8fa-4942-92e4-a59ab6a74d5d
---

# 多模态数据库索引重构计划

## 四个核心架构决策

### 1. 分层架构
- **算法层**：纯算法逻辑，通过回调访问数据（Faiss 层）
- **存储层**：页面管理器/Buffer Pool，通过配置选择后端

```c
typedef struct index_config {
    storage_backend_type_t storage_type;  // 内存/页面/mmap/Faiss
    bool persist_enabled;                  // 持久化开关
    // ...
} index_config_t;
```

### 2. 持久化开关
- `persist_enabled=true`：完整 MVCC + WAL + Redo
- `persist_enabled=false`：纯内存 + Undo（无崩溃恢复）

### 3. 统一向量存储
- Heap 是主存储（Single Source of Truth）
- 索引只存 id/指针，膨胀率从 ~2.2x 降至 ~1.2x
- 查询时：图搜索 → 获取 id → 从 Heap 查向量 → 重排序

### 4. 文档体系
- `docs/index/theory/`：索引原理（理论为主）
- `docs/index/implementation/`：索引实现（代码为主）

## 研究计划

| Phase | 索引类别 | 数量 | 状态 |
|-------|----------|------|------|
| A | 向量索引 | 7种 | 待研究 |
| B | 树索引 | 7种 | 待研究 |
| C | 哈希索引 | 5种 | 待研究 |
| D | 倒排/全文索引 | 4种 | 待研究 |
| E | 特殊索引 | BRIN/Hilbert/MVCC | 待研究 |

设计文档：`docs/openspec/specs/2026-07-14-multimodal-index-redesign.md`
