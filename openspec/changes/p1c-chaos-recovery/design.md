# P1-C 设计（详见 plan）

**关联 proposal：** `proposal.md`

## 核心思路

P1-B 把 vector_api 接入 WAL + CheckpointManager，chaos 3 失败转绿。P1-C 在此基础上做两层加固：

```
                         ┌────────────────────────────────────────┐
                         │  vector_api_create(interval_ms=5000)    │  ← 新增参数
                         │  vector_api_set_checkpoint_interval()   │  ← 新增 setter
                         └─────────────┬──────────────────────────┘
                                       │
                         ┌─────────────▼──────────────────────────┐
                         │  CheckpointManager (interval_ms 可调)   │
                         │  默认 5s，chaos 测试 100ms 跑 stress     │
                         └─────────────┬──────────────────────────┘
                                       │
       ┌───────────────────────────────┼───────────────────────────────┐
       │                               │                               │
┌──────▼──────┐               ┌────────▼────────┐              ┌───────▼──────┐
│ vdb_chaos    │               │  chaos_e2e      │              │  vdb_checkpt │
│ 6 → 8 用例   │               │  5 新用例        │              │  6 用例      │
│ + 短 interval│               │  多种 kill 模式  │              │  (P1-B)      │
└──────────────┘               └─────────────────┘              └──────────────┘
```

## 数据契约

### vector_api_create interval 参数

| 字段 | 类型 | 默认 | 说明 |
|------|------|------|------|
| `interval_ms` | `uint32_t` | `5000` | CheckpointManager 周期（毫秒），0 表示不开后台 checkpoint |

**约束**：
- 0 → 禁用 CheckpointManager（仅手动 checkpoint / save）
- < 100ms → 警告（IO 压力过大）
- 默认 5000ms 保持 P1-B 行为不变

### vector_api_set_checkpoint_interval setter

```c
int vector_api_set_checkpoint_interval(VectorAPI *api, uint32_t interval_ms);
```

**行为**：
- 运行时调：stop 当前 mgr → create 新 mgr with 新 interval → start
- 返回 0 成功，-1 失败（mgr 未运行 / 内存不足）
- 线程安全（需加锁）

### chaos_e2e 5 用例

| # | 名称 | 场景 | 验收 |
|---|------|------|------|
| 1 | KillDuringCheckpoint | 100ms 间隔，insert 后立即 destroy（让 kill 撞上 flush+checkpoint+truncate 中途） | reload 后 size >= 0，无 corrupt |
| 2 | KillDuringReplay | 启动后立刻 destroy（replay 还没读完就被 kill） | 再次 create 时继续 replay，最终 size 一致 |
| 3 | PartialWriteRecovery | 手动 truncate WAL 末尾 16 字节（模拟部分写） | reload 仍能恢复已写入的数据，损坏部分被丢弃 |
| 4 | ShortIntervalStress | 100ms 间隔 + 200 次 insert+kill 循环 | 全程零数据丢失，最终 size == 200 |
| 5 | ConcurrentSaveCrash | 4 线程写 + 1 线程 save（5s 期间反复 save） + 1 线程每 200ms 调 destroy | 每次 reload 后 size 一致（无负数 / 不超过预期） |

## 风险

- **短 interval IO 风暴**：100ms 间隔 + 大量 insert 触发 flush+checkpoint 频繁，CI 跑时间会变长。缓解：默认 5s 不变，仅 chaos 测试用 100ms
- **kill-during-checkpoint 竞态**：CheckpointManager 持有 WAL mutex，destroy 不一定能撞上 flush 窗口。缓解：循环跑 N 次取最坏情况
- **partial write 误判**：手动 truncate 末尾 16 字节可能落在有效 record header 中间，被误判为 corrupt 而丢数据。缓解：测试只 truncate 最后 16 字节（一般 < 一个 record），验证 truncate 后的 record 不被 replay
- **setter 线程安全**：需 VectorAPI_s 加 mutex 保护 interval 设置

## 实施组织

1. **Task 1**：建 OPSX 变更骨架（本目录）+ plan + feature 分支
2. **Task 2**：探查 chaos 现状 + 间隔参数化路径
3. **Task 3**：写 5 个 chaos_e2e 红测试 + CMakeLists 注册
4. **Task 4**：CheckpointManager 间隔参数化（vector_api_create 加参数 + setter）
5. **Task 5**：实现 chaos_e2e 5 + vdb_chaos 扩展 2-3
6. **Task 6**：跑 chaos 全套 + 回归 + 收口 + 合并 main

## 关键架构决策

（待 Task 2 探查后补充）
