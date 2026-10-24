# C4-2 可维护性专项 任务清单

## 任务列表

### ASAN/UBSAN CI
- [x] **T1** GitHub Actions Linux runner 配置（Ubuntu + gcc/clang + ASAN + UBSAN）
- [x] **T2** ASAN/UBSAN 全测试套绿灯基线
- [x] **T3** Windows Debug 双分配器校验维持（MinGW 无 libasan 不动）

### Release UAF 清零
- [x] **T4** 8 个存量 UAF 逐一复现测试
- [x] **T5** 逐个修复（Debug 双分配器 → ASAN 验证 → Release 验证）
- [x] **T6** Release 构建 30 天零 UAF 监控

### HNSW 三路收敛
- [x] **T7** vector_index_vtable_t 统一接口定义
- [x] **T8** 删除 storage/vector/faiss_hnsw_stub.c + index/vector_index/hnsw/hnsw_placeholder.c
- [x] **T9** 旧头 24 处引用迁移
- [x] **T10** grep 验证零命中

### 跨模态 E2E 测试
- [x] **T11** 基线 1：Graph + Vector + RAG 混合检索
- [x] **T12** 基线 2：Relational MVCC + WAL 事务
- [x] **T13** 基线 3：Vector 并发插入搜索（回归）

### DESIGN.md 同步
- [x] **T14** 8 模态 DESIGN.md 审查与实现同步
- [x] **T15** 各模态公开 API/索引/算法章节更新

### 收尾
- [x] **T16** Verify + Archive
