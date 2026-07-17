# S7 Spec —— 双轨完整性

## 1. ds/ 头文件契约

`learning/include/ds/{linked_list,stack,hash_table}.h` 声明 3 个 demo 函数：

- `void ds_linked_list_demo(void);`
- `void ds_stack_demo(void);`
- `void ds_hash_table_demo(void);`

每个声明必须有 1 个对应 .c 实现，位于：
- `learning/ds-c/orig/{linked_list,stack,hash}/impl/ds_*.c`

实现必须用独立类型（`ds_list_t`、`ds_stack_t`、`ds_ht_t`），**不得** 引用 algo-prod 的 `list_t`/`stack_t`/`map_t`。

## 2. ds_demo .c 跨轨引用禁令

实现文件不得 include 任何工程层路径：
- ❌ `#include <algo-prod/...>`
- ❌ `#include <db/...>`
- ❌ `#include <rag/...>`
- ❌ `#include <vector_index/...>`
- ❌ `#include <faiss/...>`

可用引用（学习层内部）：
- ✅ `<uthash/uthash.h>` —— 已迁入 `learning/include/uthash/`
- ✅ `<ds/...>` —— 学习层公共头

## 3. 工程层库命名一致性

所有 `target_link_libraries(... algo ...)` 应改为 `algo-prod`。S4 已重命名，但遗留 2 处（vector_index test）未同步。S7 完成。

## 4. vector_index 测试启用

`engineering/test/db/index/vector_index/CMakeLists.txt` 重新 enabled（lib link 错位修复后）。测试目标：

- `vector_reranker_test.exe` —— ReRanker 模块 1+ 个测试
- `vector_delete_test.exe` —— Delete + GC + Bitmap + Rebuild 4 测试

## 5. 不做（明确范围外）

- ❌ 重建 algo-prod 类型替代品
- ❌ ds_demo 引入 GTest 单测
- ❌ 解开 distributed/、redis/、rag/ 等其他注释
