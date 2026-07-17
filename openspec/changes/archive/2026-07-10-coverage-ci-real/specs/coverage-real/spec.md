# S17 Spec —— Coverage CI Real Wiring

## 1. Job 契约

`.github/workflows/ci.yml` 必须含 `coverage-ubuntu` job：

- 安装 lcov + PyYAML
- configure with `-DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug`
- build with `--parallel 4`
- run ctest (`|| true` 不让 CI 红)
- 跑 `scripts/coverage/run.sh`
- 跑 `aggregate.py` → coverage-current.json
- 跑 `compare.py` → coverage-diff.md
- 跑 `badge.py` → coverage-badge.svg
- upload artifacts

## 2. 不让 CI 红

每个可失败步骤加 `|| true`，coverage 是 informational report，不是测试 gate。

## 3. Artifacts 上传

上传到 `coverage-report`：
- `coverage-html/`（完整 HTML 报告）
- `coverage.info`（原始 lcov）
- `coverage.info.filtered`（过滤后）
- `coverage-current.json`（当前快照）
- `coverage-diff.md`（与 baseline 对比）
- `coverage-badge.svg`（徽章）

## 4. 不做

- ❌ 强制 coverage 阈值
- ❌ PR 评论
- ❌ Codecov / Coveralls
