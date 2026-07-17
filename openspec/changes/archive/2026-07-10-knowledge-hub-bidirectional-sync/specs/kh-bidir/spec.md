# P3 Spec —— Knowledge_Hub Bidirectional Sync

## 1. 同步方向

- forward：learning/notes/learn_deep/ → knowledge_hub (默认)
- reverse：knowledge_hub → learning/notes/learn_deep/
- both：先 reverse 后 forward（最终一致）

## 2. sync state

`.sync-state.json` 在主存储目录内：
```json
{
  "last_forward_ms": 1720000000000,
  "last_reverse_ms": 0,
  "version": "1"
}
```

## 3. conflict 检测

sync 时若目标端 mtime > last_sync_ms → 报告冲突，保留较新者。

## 4. 子命令

- `bash sync-learn-deep.sh init-state` — 初始化
- `bash sync-learn-deep.sh status` — 列出待同步文件
- `bash sync-learn-deep.sh` — forward
- `bash sync-learn-deep.sh --reverse` — reverse
- `bash sync-learn-deep.sh --both` — 双向

## 5. 不做

- ❌ conflict 解决（仅报告）
- ❌ fs watch
