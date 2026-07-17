# P2 — Design（Coverage HTML Report）

## 1. lcov 工具链检测

`run.sh` 头部：

```bash
if command -v lcov >/dev/null; then
    LCOV_MODE=1
else
    LCOV_MODE=0
fi
```

## 2. lcov 模式（CI）

```bash
lcov --capture \
    --directory engineering/build-cov \
    --output-file coverage.info \
    --rc lcov_branch_coverage=1 \
    --ignore-errors mismatch

lcov --extract coverage.info "${REPO}/engineering/src/*" \
    --output-file coverage.filtered.info

genhtml coverage.filtered.info \
    --output-directory engineering/coverage-html \
    --branch-coverage=1 --legend
```

## 3. gcov 模式（Windows / 本地 fallback）

继续使用 S24 的 `aggregate-gcov.py` 流程：
- `gcov` 收集 .gcov
- `aggregate-gcov.py` 解析 → coverage-baseline.json
- 不生成 HTML

## 4. 脚本结构

```
engineering/scripts/coverage/
├── run.sh                 # 主入口
├── aggregate-gcov.py      # gcov-only Python 解析
├── lcov-aggregate.sh      # lcov 完整流程
├── aggregate.py           # 已存在（按 lcov .info 解析）
└── compare.py             # 已存在
```

## 5. lcov-aggregate.sh 设计

```bash
#!/usr/bin/env bash
# 仅在 lcov 可用时跑
set -euo pipefail
lcov --capture --directory engineering/build-cov \
    --output-file coverage.info \
    --rc lcov_branch_coverage=1 --ignore-errors mismatch

lcov --extract coverage.info \
    "${REPO}/engineering/src/*" \
    --output-file coverage.filtered.info

genhtml coverage.filtered.info \
    --output-directory engineering/coverage-html \
    --branch-coverage=1 --legend
```

## 6. README 链接更新

```markdown
[Coverage HTML Report](engineering/coverage-html/index.html)
```

## 7. 验证 V1-V3

| V | 命令 |
|---|---|
| V1 | Ubuntu CI 跑 lcov-aggregate.sh 成功 |
| V2 | coverage-html/index.html 非空 |
| V3 | 工程层 coverage > 0% 含 |
