# S12 — Tasks (CI Dual-Track Integration)

> **目标**：让 GitHub Actions CI 自动验证双轨独立构建 + 测试，覆盖 ~256 个 ctest。

## 1.1 调研

- [x] 1.1.1 已查：现有 `.github/workflows/ci.yml` 4 jobs，3 个引用已删除脚本/路径
- [x] 1.1.2 已查：cmake -B build 即可触发双轨（依赖 -DENGINEERING/LEARNING_BUILD）

## 1.2 重写 ci.yml

- [ ] 1.2.1 删 coverage + analyze jobs
- [ ] 1.2.2 重写 build-engineering-ubuntu job
- [ ] 1.2.3 重写 build-engineering-windows job（仅 build，不跑 ctest）
- [ ] 1.2.4 新增 build-learning-ubuntu job
- [ ] 1.2.5 新增 test-engineering-ubuntu job（depends on build-engineering）
- [ ] 1.2.6 新增 test-learning-ubuntu job（depends on build-learning）

## 1.3 验证 V1-V4

- [ ] 1.3.1 V1: ci.yml YAML 合法（yamllint 可选）
- [ ] 1.3.2 V2: 工程层 build 命令本地模拟成功
- [ ] 1.3.3 V3: 学习层 build 命令本地模拟成功
- [ ] 1.3.4 V4: 提交后 CI run 自动触发（推送验证）

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/ci-dual-track-integration/ .github/workflows/ci.yml`
- [ ] 1.4.2 `git commit -m "ci(dual-track): 双轨 CI 集成——独立 build + test jobs"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-ci-dual-track-integration/`
