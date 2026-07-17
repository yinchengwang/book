# S21 Spec —— Coverage Threshold

## 1. 阈值契约

| 模块 | lines_pct 阈值 | 失败时 |
|---|---|---|
| db-core / db-storage-engine | ≥ 80% | exit 1 |
| algo-prod / vector_index / rag | ≥ 50% | exit 1 |
| 其他 | 不强求 | warning |

## 2. 脚本输出

失败时打印 Markdown 表格：

```
| 模块        | 当前 | 阈值 | 状态 |
|------------|------|------|------|
| db-core    | 65%  | 80%  | ❌ |
| algo-prod  | 72%  | 50%  | ✅ |
```

## 3. CI 集成

coverage-ubuntu job 末尾添加：

```yaml
- name: Check coverage thresholds
  working-directory: ./engineering
  run: python3 scripts/coverage/check-threshold.py coverage-current.json
```

不设 `if: always()`：失败让 CI 红（让工程师必须修）

## 4. 不做

- ❌ Slack/Discord webhook
- ❌ PR 评论
- ❌ Coverage 历史趋势
