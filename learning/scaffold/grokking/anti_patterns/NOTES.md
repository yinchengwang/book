# 反模式 学习笔记

## 反模式类型

| 反模式 | 违反原则 | 症状 | 重构方案 |
|--------|----------|------|----------|
| GodClass | 单一职责 (SRP) | 一个类 > 200 行, 包含不相关的字段/方法 | 职责分离为多个类, 通过组合协作 |
| HardCoded | DRY / 开闭 (OCP) | 魔法数字散布在各处, 修改需搜索全部 | 提取为命名常量或配置文件 |
| Spaghetti | 最小知识 / 无副作用 | 多层嵌套 + 全局变量, 无法单元测试 | 纯函数 + pipeline 模式 |

## 工程对照

### engineering/test/ — GodClass 反模式的天然对策

`engineering/test/` 目录下拥有 **40+ 个测试文件**, 每个测试遵循 Arrange-Act-Assert 三段式结构。这种测试模式天然抵制 GodClass 反模式: 测试必须针对单一模块/函数进行, 如果一个函数干太多事, 测试将变得极其复杂。例如 `engineering/test/heapam_test.cpp` 只测试堆表操作的增删改查, 不会混入 BTree 索引或 WAL 日志的测试逻辑。测试的模块化倒逼了生产代码的职责分离。

### engineering/src/db/ — 分层架构对抗 GodClass

`engineering/src/db/` 的模块化分层是消除 GodClass 的典范:

- **WAL** (`wal.c`, `wal_buf.c`) — 写前日志, 只负责持久化和恢复
- **Buffer Pool** (`bufmgr.c`) — 页面缓存, 只负责内存管理与脏页刷盘
- **Access Method** (`rel.c`) — Relation 抽象, 只负责表扫描接口
- **Heap AM** (`heapam.c`) — 堆表存储, 只负责元组操作

每一层职责清晰, 不越界。如果把这些逻辑全塞进一个 `GodDatabase.c`, 那将是一场灾难。

### cmake/ProjectUtils.cmake — DRY 原则的体现

`cmake/ProjectUtils.cmake` 提供了 `add_project_test()` 和 `add_project_library()` 两个辅助宏, 消除 CMakeLists.txt 中的重复模式。如果每个子目录都手写 `file(GLOB ...)` + `add_executable(...)` + `gtest_discover_tests(...)`, 就是 HardCoded 反模式在构建系统中的表现。这里的 DRY 通过宏抽象消除重复, 和反模式中命名常量的思路一致。

## 面试要点

1. **区别反模式和坏味道**: 反模式是反复出现的"伪解决方案", 坏味道是代码可能需要重构的信号
2. **GodClass 的典型阈值**: 当一个类的内聚度降低 (字段被不同方法操作且互不关联), 就该拆分
3. **Spaghetti vs Big Ball of Mud**: 面条式代码侧重于控制流混乱, 大泥球侧重于架构层面无结构
4. **重构策略**: 先加测试 -> 小步重构 -> 每个重构只改一件事 (与项目中 OPSX 变革纪律一致)
