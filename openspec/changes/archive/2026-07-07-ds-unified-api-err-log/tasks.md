# 任务列表

## ✅ 已完成：统一错误码与日志系统

- [x] 创建 `include/common/base_err.h` - SYS_* 错误码 + 基础错误结构
- [x] 创建 `include/common/base_log.h` - 统一日志宏
- [x] 创建 `include/ds/ds_err.h` - DS_* 数据结构错误码
- [x] 创建 `include/db/db_err.h` - DB_* + SQLSTATE 兼容码
- [x] 创建 `include/rag/rag_err.h` - RAG_* 错误码

## ✅ 已完成：DS API 修复

- [x] 修复 Queue API：`dequeue()` 统一为 `dequeue(queue, &data)` 出参风格
- [x] 修复 `array_queue_length` 计算错误（使用 size 字段）
- [x] 修复 `queue_node_t` vs `link_list_node_t` 类型不一致
- [x] 添加链表节点 `pre` 指针支持
- [x] 补充 Tree API 便利函数到 `algo/ds/tree.h`
- [x] 补充 String API 缺失函数到 `algo/ds/str.h`
- [x] 添加 `TO_VOID_PTR`、`STATUS_OK` 等宏到 `data_type.h`
- [x] 更新测试文件：`str_test.cpp`、`tree_test.cpp`、`queue_test.cpp`
- [x] 移除 CMakeLists.txt 中的排除列表
- [x] 更新 ds 库 CMakeLists.txt 包含 tree.c
- [x] 修复 `str.c` 中的 time_unit bug（"hour" → "year"）
- [x] 修复测试期望值（与实际常量对齐）
- [x] **31/31 测试全部通过**

## ⚠️ 后续实施：旧文件废弃

- [ ] 废弃 `include/db/errors.h`（仍有 5 个文件引用，待迁移）
- [ ] 废弃 `include/db/log.h`（需检查引用）
- [x] 废弃 `rag/include/rag/error.h`（已删除）
- [x] 废弃 `rag/include/rag/logger.h`（已删除）
