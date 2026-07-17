# Git 命令速查表

## 基础工作流

```bash
# 初始化 / 克隆
git init                          # 新建仓库
git clone <url>                   # 克隆远程仓库

# 暂存与提交
git status                        # 查看工作区状态
git add <file>                    # 暂存文件
git add -p                        # 交互式选择暂存块
git commit -m "<msg>"             # 提交
git commit --amend                # 修改最后一次提交

# 历史查看
git log --oneline --graph         # 简洁图形化日志
git log -p <file>                 # 查看某文件的提交历史+diff
git blame <file>                  # 逐行追溯谁改的
git show <commit>                 # 查看某次提交的完整 diff

# 分支操作
git branch <name>                 # 创建分支
git switch <name>                 # 切换分支（Git 2.23+）
git checkout -b <name>            # 创建+切换（旧语法）
git merge <branch>                # 合并分支
git rebase <branch>               # 变基到目标分支
git branch -d <name>              # 删除已合并分支
```

## 撤销与恢复

```bash
git restore <file>                # 撤销工作区改动
git restore --staged <file>       # 取消暂存
git reset --soft HEAD~1           # 撤销 commit，保留暂存
git reset --hard HEAD~1           # 完全撤销最近一次 commit（危险！）
git revert <commit>               # 安全撤销——新建反向 commit
git stash                         # 暂存当前修改
git stash pop                     # 恢复最近一次 stash
```

## 远程协作

```bash
git remote -v                     # 列出远程仓库
git fetch <remote>                # 下载远程更新（不合并）
git pull                          # = fetch + merge
git pull --rebase                 # = fetch + rebase（推荐）
git push <remote> <branch>        # 推送到远程
git push --force-with-lease       # 安全强制推送（拒绝覆盖他人工作）
```

## 调试与高级

```bash
git bisect start                  # 开始二分查找 bug
git bisect bad / good             # 标记坏/好 commit
git bisect run <test-script>      # 自动二分查找
git bisect reset                  # 结束二分查找
git cherry-pick <commit>          # 摘取某次 commit 到当前分支
git reflog                        # 查看所有 HEAD 移动历史（救回"丢失"的 commit）
git tag v1.0.0                    # 创建轻量标签
git tag -a v1.0.0 -m "msg"       # 创建附注标签
```

## hash-object 演示（理解 Git 内部）

```bash
# Git 的核心是 content-addressable 文件系统
echo "hello world" | git hash-object --stdin
# 输出: 3b18e512dba79e4c8300dd08aeb37f8e728b8dad

# 存入 object store
echo "hello world" | git hash-object -w --stdin

# 查看 object
git cat-file -p 3b18e512dba79e4c8300dd08aeb37f8e728b8dad
# 输出: hello world
```
