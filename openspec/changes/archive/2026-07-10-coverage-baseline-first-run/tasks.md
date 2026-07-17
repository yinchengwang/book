# S24 — Tasks（Coverage Baseline First Run）

> **目标**：第一次跑 real coverage baseline，让 baseline JSON 数字从 0% 变成真实数字。

## 1.1 调研

- [x] 1.1.1 已查：lcov 不在 Windows（默认），需支持 gcov-only
- [x] 1.1.2 已查：gcov 在 mingw64 可用

## 1.2 实施

- [ ] 1.2.1 创建 `engineering/scripts/coverage/aggregate-gcov.py`
- [ ] 1.2.2 跑一次 baseline：用 gcov-only 模式
- [ ] 1.2.3 更新 `engineering/docs/coverage-baseline.json` 真实数字

## 1.3 验证 V1-V3

- [ ] 1.3.1 V1: aggregate-gcov.py 解析 gcov 输出
- [ ] 1.3.2 V2: baseline JSON 含 lines_pct > 0
- [ ] 1.3.3 V3: 双轨依然 158 + 工程层 ctest 全过

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/coverage-baseline-first-run/ engineering/scripts/coverage/aggregate-gcov.py engineering/docs/coverage-baseline.json`
- [ ] 1.4.2 `git commit -m "feat(coverage): 第一次真实 baseline"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-coverage-baseline-first-run/`
