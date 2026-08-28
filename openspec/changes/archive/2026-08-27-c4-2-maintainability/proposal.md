# C4-2 可维护性专项 Proposal

## Why

差距分析 README §1.4 四维度目标之一。现状：Release 构建 8 个存量 use-after-free（memory: p6 记录，Debug 全绿但 Release 不稳定）；无常态化 ASAN/UBSAN CI（MinGW 无 libasan，Task 56 跳过）；三套 HNSW 路径并存（storage/vector/faiss_hnsw_stub.c + index/vector_index/faiss_hnsw/ + index/vector_index/hnsw/placeholder）；跨模态端到端集成测试稀疏。

## What Changes

- **ASAN/UBSAN CI**：GitHub Actions Linux runner（Ubuntu）跑全测试套（MinGW 无 libasan，Windows 侧维持 Debug 双分配器校验）
- Release UAF 8 个存量逐一修复（先稳定复现测试再修）
- **三套 HNSW 路径收敛为一**：
  - `vector_index_vtable_t` 统一接口
  - 删除 `storage/vector/faiss_hnsw_stub.c`
  - 删除 `index/vector_index/hnsw/hnsw_placeholder.c`
  - 旧头文件 24 处引用迁移到新抽象
- **跨模态 E2E 测试基线**：
  - 基线 1：Graph + Vector + RAG 混合检索
  - 基线 2：Relational MVCC + WAL 事务
  - 基线 3：Vector 并发插入搜索（复现 C1-2 修复后回归）
- **DESIGN.md 同步**：每模态设计文档随变更更新（faiss_hnsw/DESIGN.md、streaming/DESIGN.md 模式推广到所有模态）
- **错误路径清理自动化**：执行器初始化 MemoryContext 化（C0-3 部分）+ Yang arena（C2-5 部分）后的统一审计

## Capabilities

| 能力 | 交付 |
|------|------|
| ASAN CI | Linux 侧 ASAN/UBSAN 全测试套绿灯 |
| UAF 清零 | Release 构建 0 use-after-free（持续 30 天） |
| HNSW 收敛 | `grep -r faiss_hnsw_stub\|hnsw_placeholder` 零命中 |
| 跨模态 E2E | 三基线测试稳定通过 |
| DESIGN 同步 | 全部 8+ 模态 DESIGN.md 与实现同步 |

## Impact

- 修改：CI 配置（`.github/workflows/`）、各模块 DESIGN.md、HNSW 收敛迁移
- 新增：UAF 复现测试、跨模态 E2E 三基线
- 预计 8-12 个 commit
- 依赖：C1-2、C2-1
