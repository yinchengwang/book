# P6 — Design（lcov CI Full Integration）

## 1. CI 修改

`.github/workflows/ci.yml` 的 coverage-ubuntu job：

```yaml
- name: Checkout
- name: Install lcov + deps
- name: Build engineering with coverage
  # cmake -B engineering/build-cov -DENABLE_COVERAGE=ON
- name: Run coverage script
  working-directory: ./engineering
  run: bash scripts/coverage/run.sh
- name: Generate badge
  working-directory: ./engineering
  run: |
    if [ -f coverage-current.json ]; then
      python3 scripts/coverage/badge.py coverage-current.json coverage-badge.svg
    fi
- name: Upload artifacts (HTML + JSON + badge)
  with:
    path: |
      engineering/coverage-html/
      engineering/coverage.info.filtered
      engineering/coverage-current.json
      engineering/coverage-badge.svg
```

## 2. Thresholds 不强制

P0 不强制（信息性报告）
P6 不引入 threshold check 强制——保持 informational

## 3. 验证

| V | 命令 |
|---|---|
| V1 | ci.yml YAML 合法 |
| V2 | run.sh 模拟 CI 跑（用 lcov） |
| V3 | HTML 报告 + JSON 输出非空 |
