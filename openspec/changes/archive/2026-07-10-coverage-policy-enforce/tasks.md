# S21 — Tasks（Coverage Threshold Enforcement）

## 1.1 调研

- [x] 1.1.1 已查：coverage-policy.md 阈值：核心 80% / 其他 50%
- [x] 1.1.2 已查：S17 aggregate.py 输出 {modules: {name: {lines_pct, branches_pct, ...}}}

## 1.2 实施

- [ ] 1.2.1 创建 `engineering/scripts/coverage/check-threshold.py`
- [ ] 1.2.2 CI 中 coverage job 增加 threshold check 步
- [ ] 1.2.3 `coverage-policy.md` 增加"违规处理"段

## 1.3 验证 V1-V3

- [ ] 1.3.1 V1: check-threshold.py 用 baseline JSON 跑通
- [ ] 1.3.2 V2: 违规时 exit 1
- [ ] 1.3.3 V3: ci.yml YAML 合法

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/coverage-policy-enforce/ engineering/scripts/coverage/ .github/workflows/ci.yml engineering/docs/coverage-policy.md`
- [ ] 1.4.2 `git commit -m "ci(coverage): 阈值强制——核心 80% / 其他 50%"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-coverage-policy-enforce/`
