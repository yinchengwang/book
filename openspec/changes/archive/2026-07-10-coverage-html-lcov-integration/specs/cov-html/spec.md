# P2 Spec —— Coverage HTML 报告

## 1. 双模式入口

`run.sh` 检测 lcov：
- **可用**：走 lcov + genhtml 模式（CI Ubuntu）
- **不可用**：走 gcov + aggregate-gcov.py 模式（本地 Windows fallback）

## 2. lcov-aggregate.sh 契约

```bash
lcov --capture --directory engineering/build-cov --output-file coverage.info --ignore-errors mismatch
lcov --extract coverage.info "${REPO}/engineering/src/*" --output-file coverage.filtered.info
genhtml coverage.filtered.info --output-directory engineering/coverage-html --branch-coverage=1
```

输出 `engineering/coverage-html/index.html`。

## 3. README 链接

增加段：
```
[Coverage Report](engineering/coverage-html/index.html)
```

## 4. 不做

- ❌ Windows genhtml
- ❌ Codecov 上传
- ❌ PR 评论
