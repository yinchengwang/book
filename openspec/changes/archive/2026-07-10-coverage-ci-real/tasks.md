# S17 — Tasks (Coverage CI Real)

> **目标**：把现有 coverage 工具链真实接入 GitHub Actions CI。

## 1.1 调研

- [x] 1.1.1 已查：`scripts/coverage/{run.sh,aggregate.py,compare.py,badge.py}` 完整
- [x] 1.1.2 已查：`ENABLE_COVERAGE` 在 CMakeLists 已定义
- [x] 1.1.3 已查：S12 已删除旧 CI coverage job

## 1.2 实施

- [ ] 1.2.1 修改 `.github/workflows/ci.yml`：新增 `coverage-ubuntu` job
- [ ] 1.2.2 job 包含：install lcov, build with -DENABLE_COVERAGE, ctest, run.sh, aggregate, compare, badge, artifacts

## 1.3 验证 V1-V4

- [ ] 1.3.1 V1: ci.yml YAML 合法
- [ ] 1.3.2 V2: 本地模拟 run.sh 跑通
- [ ] 1.3.3 V3: aggregate 解析 output 有效
- [ ] 1.3.4 V4: push 后 CI 看到 artifacts

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/coverage-ci-real/ .github/workflows/ci.yml`
- [ ] 1.4.2 `git commit -m "ci(coverage): 真正接入 coverage 工具链到 GitHub Actions"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-coverage-ci-real/`
