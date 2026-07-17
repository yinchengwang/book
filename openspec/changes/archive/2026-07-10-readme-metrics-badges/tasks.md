# S20 — Tasks（README Badges + Metrics）

> **目标**：让 README metrics badges 自动反映真实数字。

## 1.1 调研

- [x] 1.1.1 已查：README badges 当前手填 / TBD
- [x] 1.1.2 已查：metrics 收集工具不存在

## 1.2 实施

- [ ] 1.2.1 创建 `engineering/scripts/metrics/collect.sh`：扫 LOC / 测试数 / 双轨状态
- [ ] 1.2.2 创建 `docs/project-metrics.json`：metrics snapshot
- [ ] 1.2.3 更新 README 用真实指标替换 TBD

## 1.3 验证 V1-V4

- [ ] 1.3.1 V1: collect.sh 跑通
- [ ] 1.3.2 V2: project-metrics.json 含预计字段
- [ ] 1.3.3 V3: README badges 数字真实
- [ ] 1.3.4 V4: 与现网 ctest 数一致

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/readme-metrics-badges/ docs/ engineering/scripts/metrics/ README.md`
- [ ] 1.4.2 `git commit -m "metrics(badges): README 综合指标 + 自动收集"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-readme-metrics-badges/`
