# P5 — Design（Raft Persistence）

## 1. 序列化格式

`<path>/raft-state.bin`：

```
[magic 8B: "RAFTv0.1"]
[term uint64_le]
[voted_for uint64_le]
[log_size uint64_le]
[for each entry:
   [term uint64_le]
   [data_size uint64_le]
   [data bytes]
]
```

## 2. API

```c
typedef struct RaftStateConfig {
    const char *state_path;  /* NULL = in-memory */
} RaftStateConfig_t;

/* 装载：create 时从 state_path 读 */
int raft_server_create_ex(const RaftServerConfig_t *cfg,
                         const RaftStateConfig_t *state_cfg,
                         RaftServer_t **out);

/* 保存：每次 tick + submit 后调用 */
int raft_server_save_state(RaftServer_t *srv);

/* 手动触发 */
int raft_server_load_state(RaftServer_t *srv, const char *path);
```

## 3. 实现策略

`raft.c` 加：
- `save_lock` (mutex)
- `state_path`（来自 RaftStateConfig）
- `save_dirty` flag（set 后调 save）

每次 `tick()` 结束 / `raft_submit()` 成功 → set dirty → backend thread 持久化。

P5 简化：**同步保存**（每次 tick 后立即 save），避免 backend thread 复杂度。

## 4. file format

```c
// engineering/src/db/consensus/raft_persist.c
static int write_state(const char *path, const struct RaftServer *srv) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    fwrite("RAFTv0.1", 1, 8, fp);
    uint64_t term = srv->current_term;
    uint64_t voted = srv->voted_for;
    uint64_t size = srv->log_size;
    fwrite(&term, sizeof(term), 1, fp);
    fwrite(&voted, sizeof(voted), 1, fp);
    fwrite(&size, sizeof(size), 1, fp);
    for (size_t i = 0; i < srv->log_size; i++) {
        fwrite(&srv->log[i].term, sizeof(uint64_t), 1, fp);
        fwrite(&srv->log[i].data_size, sizeof(size_t), 1, fp);
        fwrite(srv->log[i].data, srv->log[i].data_size, 1, fp);
    }
    fclose(fp);
    return 0;
}
```

## 5. test 场景

```cpp
TEST(RaftPersist, RestartRecoversTerm) {
    auto cfg = ...;
    RaftServer *srv1 = raft_server_create_ex(&cfg, &state_cfg, nullptr);
    raft_server_save_state(srv1);
    raft_server_destroy(srv1);

    RaftServer *srv2 = raft_server_create_ex(&cfg, &state_cfg, nullptr);
    EXPECT_EQ(raft_get_current_term(srv2), term_prev);
}
```

## 6. 验证 V1-V3

| V | 命令 |
|---|---|
| V1 | `cmake --build engineering/build` |
| V2 | raft_persist_test.exe 6+ tests |
| V3 | ctest -R RaftPersist 全部 pass |

## 7. 不做

- ❌ Snapshot
- ❌ 异步后台 thread
- ❌ WAL integration（独立工作）
- ❌ log compaction
