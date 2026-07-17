# P6 — Tasks（lcov CI Full Integration）

## 1.1 调研

- [x] 1.1.1 已查：CI 中 coverage job 已存在（S17 写）
- [x] 1.1.2 已查：run.sh + lcov-aggregate.sh 都已存在

## 1.2 实施

- [ ] 1.2.1 修改 ci.yml：coverage job 跑 run.sh + 生成 badge
- [ ] 1.2.2 README 链接徽章

## 1.3 验证 V1-V3

- [ ] 1.3.1 V1: ci.yml YAML 合法
- [ ] 1.3.2 V2: run.sh 模拟
- [ ] 1.3.3 V3: badge 数字正确

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/lcov-ci-full-integration/ .github/workflows/ci.yml README.md`
- [ ] 1.4.2 `git commit -m "ci(coverage): P6 lcov 完整 CI 接入"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-lcov-ci-full-integration/`
