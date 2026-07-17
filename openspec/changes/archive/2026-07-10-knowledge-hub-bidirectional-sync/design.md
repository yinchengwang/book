# P3 — Design（Knowledge_Hub Bidirectional Sync）

## 1. sync 状态

`.sync-state.json`（在 learning/notes/learn_deep/）：
```json
{
  "last_forward_sync_ms": 1720000000000,
  "last_reverse_sync_ms": 0,
  "files": {
    "c/README.md": {"mtime_ms": 1720000000000, "hash": "abc"}
  }
}
```

## 2. sync directions

- **forward**（默认）：learning → knowledge_hub
- **reverse**：`--reverse`：knowledge_hub → learning
- **both**：先 reverse 后 forward（合并双向）

## 3. conflict detection

当 sync 一个文件时：
1. 比较当前 mtime 与 sync_state.last_sync_ms
2. 目标端也已修改（mtime > last_sync_ms） → conflict
3. conflict 报告：列出两边都修改的文件，**保留较新者**（默认策略）

## 4. 脚本改进

`learning/scripts/sync-learn-deep.sh`：
- 加 `--reverse` 实现（rsync from→to 即可）
- 加 `--both`（先 reverse 再 forward）
- 加 `init-state` 子命令：初次建立 sync state
- 加 `status`：输出 sync 状态 + 待同步文件

## 5. 不做

- ❌ conflict 解决（仅报告）
- ❌ 实时 watch
