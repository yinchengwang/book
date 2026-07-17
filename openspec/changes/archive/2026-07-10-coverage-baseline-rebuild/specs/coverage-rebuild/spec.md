# S13 Spec —— Coverage Baseline

## 1. 文件契约

- `engineering/scripts/coverage/run.sh`：Bash 脚本，提供 `--baseline` / `--report` 子命令
- `engineering/scripts/coverage/aggregate.py`：Python 脚本，读取 .gcda 输出 JSON
- `engineering/docs/coverage-baseline.json`：基线快照，结构 `{modules: [{name, lines_pct, branches_pct}]}`

## 2. 工具链

- GCC `-fprofile-arcs -ftest-coverage` 编译标志
- gcov 收集 .gcno + .gcda
- lcov 转换 .gcda 为 .info 并 genhtml 出 HTML

## 3. 不做（明确范围外）

- ❌ PR 评论 / Codecov / Coveralls 第三方
- ❌ 覆盖率趋势（需要时序）
- ❌ Coverage 失败导致 CI 红
