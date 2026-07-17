# P5 — Tasks（Raft Persistence）

## 1.1 调研

- [x] 1.1.1 已查：phase11 + P1 已实现
- [x] 1.1.2 决策：同步保存，避免 async thread 复杂度

## 1.2 实施

- [ ] 1.2.1 `raft.h` 加 `RaftStateConfig_t` + `raft_server_create_ex`
- [ ] 1.2.2 `raft.c` 加 `state_path` + `save_lock` + save hook
- [ ] 1.2.3 `raft_persist.c`：write/read state
- [ ] 1.2.4 CMakeLists.txt 加新源

## 1.3 测试

- [ ] 1.3.1 `raft_persist_test.cpp`：4 个测试
- [ ] 1.3.2 ctest add persist target

## 1.4 验证 V1-V3

- [ ] 1.4.1 V1: build 成功
- [ ] 1.4.2 V2: test list ≥ 4
- [ ] 1.4.3 V3: 4 测试 100% pass

## 1.5 提交 + 归档

- [ ] 1.5.1 `git add -A openspec/changes/raft-persistence/ engineering/include/db/consensus/ engineering/src/db/consensus/ engineering/test/db/consensus/`
- [ ] 1.5.2 `git commit -m "feat(raft): P5 persistence + restart recovery"`
- [ ] 1.5.3 `git push origin project`
- [ ] 1.5.4 归档到 `openspec/changes/archive/2026-07-10-raft-persistence/`
