# MVCC 并发控制 - 实施计划

## 1. 项目结构与头文件

- [ ] 1.1 创建 `include/db/concurrency/mvcc.h` - MVCC 公共接口定义
- [ ] 1.2 创建 `src/db/concurrency/CMakeLists.txt` - 构建配置
- [ ] 1.3 更新 `src/db/CMakeLists.txt` 添加 concurrency 子目录
- [ ] 1.4 更新 `test/db/CMakeLists.txt` 链接 MVCC 库

## 2. 版本链实现

- [ ] 2.1 定义 `mvcc_version_t` 结构体（xmin, xmax, data, ctid, xvac）
- [ ] 2.2 实现 `mvcc_version_new()` - 创建新版本
- [ ] 2.3 实现 `mvcc_version_chain_traverse()` - 遍历版本链查找可见版本
- [ ] 2.4 实现 `mvcc_version_update()` - 更新时创建新版本
- [ ] 2.5 实现 `mvcc_version_delete()` - 删除时设置 xmax
- [ ] 2.6 定义 `undo_record_t` 结构体 - Undo 日志记录
- [ ] 2.7 实现 Undo 日志写入与回滚

## 3. 快照管理实现

- [ ] 3.1 定义 `mvcc_snapshot_t` 结构体（xmin, xmax, xip_list）
- [ ] 3.2 实现 `mvcc_snapshot_create()` - 创建事务快照
- [ ] 3.3 实现 `mvcc_snapshot_destroy()` - 销毁快照
- [ ] 3.4 实现 `mvcc_snapshot_copy()` - 复制快照
- [ ] 3.5 实现 `mvcc_active_txn_add()` - 添加活跃事务
- [ ] 3.6 实现 `mvcc_active_txn_remove()` - 移除活跃事务
- [ ] 3.7 实现快照序列化与反序列化（WAL 恢复用）

## 4. 可见性判断实现

- [ ] 4.1 实现 `mvcc_version_visible()` - 单版本可见性判断
- [ ] 4.2 实现 `mvcc_version_visible_in_chain()` - 版本链中查找可见版本
- [ ] 4.3 实现 `mvcc_check_write_conflict()` - 写冲突检测
- [ ] 4.4 实现脏读检测与处理

## 5. 垃圾回收实现

- [ ] 5.1 定义 GC 配置结构 `mvcc_gc_config_t`
- [ ] 5.2 实现 `mvcc_gc_analyze()` - 分析死亡元组
- [ ] 5.3 实现 `mvcc_gc vacuum()` - VACUUM 清理页面
- [ ] 5.4 实现 `mvcc_gc_incremental()` - 增量 GC
- [ ] 5.5 实现 Undo 日志回收
- [ ] 5.6 实现后台 VACUUM 任务调度

## 6. 与现有模块集成

- [ ] 6.1 修改 `include/db/txn.h` - 事务结构增加 MVCC 字段
- [ ] 6.2 修改 `src/db/storage/txn.c` - 事务操作使用 MVCC
- [ ] 6.3 修改 `src/db/storage/table.c` - 表行存储支持版本链
- [ ] 6.4 修改 `src/db/storage/wal.c` - WAL 记录 MVCC 信息

## 7. 测试

- [ ] 7.1 编写版本链测试 `test/db/test_mvcc_version.cpp`
- [ ] 7.2 编写快照测试 `test/db/test_mvcc_snapshot.cpp`
- [ ] 7.3 编写可见性测试 `test/db/test_mvcc_visibility.cpp`
- [ ] 7.4 编写 GC 测试 `test/db/test_mvcc_gc.cpp`
- [ ] 7.5 编写并发场景测试 - 验证读写不阻塞
- [ ] 7.6 编译并运行所有测试

## 8. 文档

- [ ] 8.1 更新 `openspec/changes/build-my-db/tasks.md` 标记任务完成
- [ ] 8.2 编写 MVCC 设计说明文档