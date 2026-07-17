# S20 Spec —— README Badges

## 1. Metrics 数据结构

`docs/project-metrics.json`：

```json
{
  "engineering": {
    "ctest_total": 200,
    "ctest_passing": 200,
    "lines_c": 0,
    "lines_cpp": 0,
    "coverage_pct": 0.0
  },
  "learning": {
    "ctest_total": 158,
    "ctest_passing": 158,
    "obsidian_notes": 132
  }
}
```

## 2. README badge 契约

```
![CI](shields.io/badge/CI-passing)
![Engineering Tests](engineering-shield)
![Learning Tests](learning-shield)
![Engineering LOC](engineering-loc-shield)
![Learning Notes](learning-notes-shield)
```

## 3. 不做

- ❌ 实时更新（先 manual 快照）
- ❌ shields.io 自托管
- ❌ trend analysis
