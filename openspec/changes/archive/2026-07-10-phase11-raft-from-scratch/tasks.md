# Phase11 — Tasks（Engineering Raft From Scratch）

> **目标**：从零实现工程层 raft 模块，加入 ctest smoke 测试。

## 1.1 调研

- [x] 1.1.1 已查：engineering/src/db/consensus/ 不存在
- [x] 1.1.2 已查：engineering/test/db/consensus/ 不存在

## 1.2 创建头文件 + 实现

- [ ] 1.2.1 创建 `engineering/include/db/consensus/raft.h`
- [ ] 1.2.2 创建 `engineering/src/db/consensus/raft.c`
- [ ] 1.2.3 创建 `engineering/src/db/consensus/CMakeLists.txt`

## 1.3 CMakeLists 注册

- [ ] 1.2.4 在 `engineering/src/db/CMakeLists.txt` 添加 `add_subdirectory(consensus)`
- [ ] 1.2.5 在 `engineering/test/CMakeLists.txt` 添加 `add_subdirectory(db/consensus)`

## 1.4 ctest 测试

- [ ] 1.4.1 创建 `engineering/test/db/consensus/CMakeLists.txt`
- [ ] 1.4.2 创建 `engineering/test/db/consensus/raft_test.cpp`

## 1.5 验证 V1-V3

- [ ] 1.5.1 V1: cmake build engineering 全部成功（包含 raft_consensus）
- [ ] 1.5.2 V2: raft_test.exe 生成
- [ ] 1.5.3 V3: 3 个 smoke 测试通过

## 1.6 提交 + 归档

- [ ] 1.6.1 `git add -A openspec/changes/phase11-raft-from-scratch/ engineering/include/db/consensus/ engineering/src/db/consensus/ engineering/test/db/consensus/ engineering/src/db/CMakeLists.txt engineering/test/CMakeLists.txt`
- [ ] 1.6.2 `git commit -m "feat(raft): phase11 工程层 raft 从零实现"`
- [ ] 1.6.3 `git push origin project`
- [ ] 1.6.4 归档到 `openspec/changes/archive/2026-07-10-phase11-raft-from-scratch/`
