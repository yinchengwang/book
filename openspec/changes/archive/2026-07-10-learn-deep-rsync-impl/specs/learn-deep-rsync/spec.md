# S20+ Spec —— Learn Deep rsync

## 1. 数据流

```
learning/notes/learn_deep/{c,cpp,db,ds,grok,illustrate}/  ←主存储
                ↓ learning/scripts/sync-learn-deep.sh [FORWARD]
apps/web/knowledge_hub/src/data/learn_deep/                ←副本
```

## 2. sync 脚本契约

```bash
sync-learn-deep.sh                # 默认 forward
sync-learn-deep.sh --reverse      # 副本 → 主
sync-learn-deep.sh --dry-run      # 只显示动作
sync-learn-deep.sh --verbose      # 详细输出
```

## 3. 行为

- 较新者（mtime newer）覆盖较旧者
- 副本中独存但主存储不存在的文件：默认保留（防止误删）
- `--reverse` 模式同步删除副本中独存的文件
- 退出码：0 成功 / 1 错误

## 4. 不做

- ❌ Git LFS
- ❌ 实时 watch（仅 manual）
- ❌ conflict resolution（手动处理）
