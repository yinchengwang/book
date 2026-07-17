# git scaffold

Git 是管理代码的工具（元工具）。本卡片展示 Git 工作流——main.c 是"被管理的样本"，Makefile 提供 Git 专属 target。

## 复现命令

```bash
cd learning/scaffold/git
gcc -Wall -Wextra -O2 -std=c11 -o git_demo main.c
./git_demo "hello world"
./git_demo          # 测试 NULL 安全（v1.2 已修复）
```

## Git 工作流演示（需在 Git Bash 中执行）

```bash
# 查看本文件的提交历史
git log --oneline -- learning/scaffold/git/main.c

# 如果已有多个 commit，查看 diff
git diff HEAD~1 -- learning/scaffold/git/main.c

# 二分查找演示（纯文本模拟，不真的跑 bisect）
make bisect-demo
```

## 关键点

- **Git 是 content-addressable 存储**：核心是 key-value store——key=SHA-1 hash of content，value=compressed object。commit/tree/blob/tag 四种对象类型
- **stage（暂存区）是 Git 的灵魂**：`git add` 把文件内容写入 object store 并更新 index——commit 只是把 index 拍成 tree 对象
- **分支是廉价的指针**：`git branch` 只是在 `.git/refs/heads/` 下写一个 41 字节文件（40 位 SHA + 换行）——不像 SVN 那样复制整个目录
- **rebase vs merge**：rebase 线性化历史（clean），但改写了 commit SHA——绝不要 rebase 已推送的分支
- **bisect 是 bug 定位的核武器**：O(log n) 次自动定位引入 bug 的 commit，不需要读代码、不需要推理——只要写一个退出码 0=good / 1=bad 的测试脚本

详见 NOTES.md 工程对照段和 CHEATSHEET.md 命令速查表。
