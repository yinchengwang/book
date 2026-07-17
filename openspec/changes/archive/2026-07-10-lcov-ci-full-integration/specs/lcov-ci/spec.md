# P6 Spec —— lcov CI Full Integration

## 1. CI job 契约

`coverage-ubuntu` job 包含：
- 安装 lcov + Python deps
- 配置 -DENABLE_COVERAGE=ON
- run.sh（自动选 lcov 模式）
- aggregate.py → coverage-current.json
- badge.py → coverage-badge.svg
- upload artifacts

## 2. 不做

- ❌ Codecov
- ❌ PR 评论
- ❌ Threshold 强制（informational）
