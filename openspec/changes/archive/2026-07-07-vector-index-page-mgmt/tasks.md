# 向量索引页管理与图去重优化 - 任务清单

## 状态说明

- `[ ]` 待完成
- `[o]` 进行中
- `[x]` 已完成

---

## Phase 1: 基础设施

### 1.1 向量页管理模块 (vector_page)

- [x] 1.1.1 创建 `include/db/storage/vector/vec_page.h` 头文件
- [x] 1.1.2 创建 `src/db/storage/vector/vec_page.c` 实现文件
- [x] 1.1.3 实现 `vector_page_pool_create()` 页池创建
- [x] 1.1.4 实现 `vector_page_alloc()` 页分配
- [x] 1.1.5 实现 `vector_page_get()` 页获取
- [x] 1.1.6 实现 `vector_page_append()` 向量追加
- [x] 1.1.7 实现 `vector_page_evict()` LRU 置换
- [x] 1.1.8 实现 `vector_page_flush/load()` 持久化
- [x] 1.1.9 更新 `CMakeLists.txt` 添加 vector_page

### 1.2 图去重模块 (graph_dedup)

- [x] 1.2.1 创建 `include/db/storage/vector/graph_dedup.h` 头文件
- [x] 1.2.2 创建 `src/db/storage/vector/graph_dedup.c` 实现文件
- [x] 1.2.3 实现 `graph_dedup_create()` 去重器创建
- [x] 1.2.4 实现 `simhash_compute()` SimHash 计算
- [x] 1.2.5 实现 `graph_dedup_is_duplicate()` 重复检测
- [x] 1.2.6 实现 `graph_dedup_mark_inserted()` 标记已插入
- [x] 1.2.7 实现 `graph_dedup_get_heap_rows()` 反向映射查询
- [x] 1.2.8 实现 `graph_dedup_persist/load()` 持久化
- [x] 1.2.9 更新 `CMakeLists.txt` 添加 graph_dedup

---

## Phase 2: 空向量过滤

### 2.1 过滤工具函数

- [x] 2.1.1 实现 `vector_is_null()` 全零检测
- [x] 2.1.2 实现 `vector_is_invalid()` NaN/Inf 检测
- [x] 2.1.3 在 `vector_engine.c` 中集成过滤逻辑

### 2.2 向量引擎集成

- [x] 2.2.1 修改 `vector_engine_insert()` 添加空向量检查
- [x] 2.2.2 添加跳过索引标记（仅 Heap 存储）

---

## Phase 3: 图去重集成

### 3.1 向量引擎去重集成

- [x] 3.1.1 在 `vector_engine_db_t` 中添加 `graph_dedup_t *` 字段
- [x] 3.1.2 修改 `vector_engine_insert()` 集成去重检查
- [x] 3.1.3 添加去重启用/禁用 API
- [x] 3.1.4 添加去重统计接口

### 3.2 持久化集成

- [x] 3.2.1 在 `vector_engine_open()` 中加载去重状态
- [x] 3.2.2 在 `vector_engine_close()` 中保存去重状态

---

## Phase 4: 测试

### 4.1 编译验证

- [x] 4.1.1 编译 vec_page_lib
- [x] 4.1.2 编译 graph_dedup_lib
- [x] 4.1.3 编译 vector_engine_lib

---

## 验证任务

- [x] 编译通过无错误
- [x] 库链接成功
- [x] 单元测试（20 个测试全部通过）
- [ ] 性能基准测试（待实现）

---

## 文件清单

```
新增头文件:
├── include/db/storage/vector/vec_page.h         # 向量页管理 API (约 200 行)
└── include/db/storage/vector/graph_dedup.h    # 图去重 API (约 250 行)

新增实现:
├── src/db/storage/vector/vec_page.c            # 向量页管理实现 (约 400 行)
├── src/db/storage/vector/graph_dedup.c       # 图去重实现 (约 600 行)

更新文件:
├── src/db/storage/vector/CMakeLists.txt        # 添加新模块
├── include/db/storage/vector/vector_engine.h # 扩展接口
├── src/db/storage/vector/vector_engine.c     # 集成去重/过滤
└── test/db/storage/vector_engine_test.cpp    # 单元测试（20 个用例）

待添加测试:
└── test/db/storage/vector_index_test.cpp      # 集成测试
```

---

**变更状态**: ✅ 核心功能完成，单元测试通过
