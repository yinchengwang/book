# C4-2 可维护性专项设计文档

## 设计目标

ASAN CI + Release UAF 清零 + HNSW 三路收敛 + 跨模态 E2E 测试 + DESIGN.md 同步。

## 方案

1. **ASAN CI**：GitHub Actions Linux runner + gcc/clang ASAN/UBSAN
2. **Release UAF**：8 个存量逐个复现测试 → 修复 → Debug/ASAN 验证
3. **HNSW 收敛**：删 faiss_hnsw_stub.c + hnsw_placeholder.c，旧头 24 处引用迁移
4. **跨模态 E2E**：3 基线（Graph+Vector+RAG / Relational MVCC+WAL / Vector 并发）
5. **DESIGN.md**：8 模态同步

## 文件

- `.github/workflows/ci-asan.yml`（新增）
- `engineering/test/db/integration/cross_modal_*.cpp`（3 个 E2E 基线）
