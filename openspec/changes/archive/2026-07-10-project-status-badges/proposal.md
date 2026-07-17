# S23 — Project Status Badges（仓库状态概览）

## What Changes

S20 加综合 metrics，**S23** 进一步添加 **项目状态 badges**：项目规模、活跃度、变更归档数等。

具体新增 5 个 badges：

```
![Repo Size](shield/Size-...)
![Commits](shield/Commits-...)
![OpenSpec Changes](shield/Changes-...)
![Coverage Engine](shield/Cov-...)
![Learning CTest](shield/Test-...)
```

## Why

**α + β 价值**：
- 仓库"门面"完整

## Scope

**包含**：
- README 增加 5 个新 badge 行
- 1 个更新脚本 `update-badges.sh`（manual trigger）

**不包含**：
- ❌ 自动更新（手动为主）
- ❌ shields.io 自托管
