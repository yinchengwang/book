# P2 — Tasks（Coverage HTML lcov 集成）

> **目标**：CI lcov 模式生成完整 HTML 报告，README 链接。

## 1.1 调研

- [x] 1.1.1 已查：S24 aggregate-gcov.py 已运行
- [x] 1.1.2 已查：S17 CI job 已 install lcov

## 1.2 实施

- [ ] 1.2.1 创建 `engineering/scripts/coverage/lcov-aggregate.sh`
- [ ] 1.2.2 修改 `engineering/scripts/coverage/run.sh`：检测 lcov 自动选择
- [ ] 1.2.3 README 增加 HTML 链接段

## 1.3 验证 V1-V3

- [ ] 1.3.1 V1: lcov-aggregate.sh 语法合法（bash -n）
- [ ] 1.3.2 V2: run.sh 检测逻辑正确
- [ ] 1.3.3 V3: README 含 coverage-html 链接

## 1.4 提交 + 归档

- [ ] 1.4.1 `git add -A openspec/changes/coverage-html-lcov-integration/ engineering/scripts/coverage/ README.md`
- [ ] 1.4.2 `git commit -m "ci(coverage): lcov HTML 报告完整流程"`
- [ ] 1.4.3 `git push origin project`
- [ ] 1.4.4 归档到 `openspec/changes/archive/2026-07-10-coverage-html-lcov-integration/`
