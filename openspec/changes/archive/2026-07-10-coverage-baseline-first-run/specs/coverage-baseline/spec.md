# S24 Spec —— Coverage Baseline First Run

## 1. 工具链兼容

S24 工具链优先顺序：
1. `lcov --capture`（Ubuntu CI）
2. `gcov + 自定义 Python 解析`（Windows 本地 fallback）

## 2. baseline 契约

`engineering/docs/coverage-baseline.json`：

```json
{
  "generated_at": "...",
  "modules": {
    "<lib>": {"lines_pct": <0-100>, "branches_pct": <0-100>, "lines_found": N, "lines_hit": N}
  }
}
```

值首次生成后**只能上升**（防止 baseline 倒退）。

## 3. 不做

- ❌ branches / functions 细分
- ❌ Codecov 上传
- ❌ 历史 trend
