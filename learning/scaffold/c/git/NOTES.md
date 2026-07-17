# git 学习笔记

## 概念地图

Git 不只是一个"保存代码"的工具——它是一套**content-addressable 文件系统**上的版本管理协议：

- **四种对象模型**：
  - **blob**：文件内容的压缩快照（没有文件名、没有路径——那是 tree 的职责）
  - **tree**：目录清单，每条记录 = (mode, type, hash, name)。tree 递归引用其他 tree 和 blob，构成完整目录树
  - **commit**：指向一个 tree（项目根目录快照）+ 父 commit(s) + 作者/时间/提交信息。每次 commit 都是**整个项目**的完整快照——这就是 Git 能做 `git checkout <any-commit>` 的原因
  - **tag**：指向某个 commit 的不可变引用（轻量标签 = 固定指针，附注标签 = 带签名和信息的 tag 对象）
- **三棵树模型**：HEAD（上次 commit 的快照）→ Index（暂存区，下次 commit 的内容）→ Working Directory（你正在编辑的文件）。`git status` 就是比较这三棵树的差异
- **引用（refs）**：`refs/heads/main`、`refs/remotes/origin/main`、`refs/tags/v1.0`——全是指向 commit 的 40 字节指针。HEAD 是"当前引用"的别名（通常指向 `refs/heads/<branch>`）
- **DAG（有向无环图）**：commit 的父子关系构成 DAG——merge commit 有两个父节点（bifurcation→convergence）。`git log --graph` 就是在可视化这个 DAG

## 踩坑记录

1. **detached HEAD 不可怕**：当你 `git checkout <commit>` 而非分支名时，HEAD 直接指向 commit（"游离状态"）。此时做 commit 不关联任何分支——切换分支后这些 commit 就"丢失"了（但 `git reflog` 能找回来）
2. **merge conflict 的本质**：两个分支修改了同一文件的相同行——Git 无法自动决定保留哪个。冲突标记 `<<<<<<<` / `=======` / `>>>>>>>` 是在说"你来做决策"
3. **`git reset --hard` 的危险**：它直接移动 HEAD 和分支指针，不保留工作区——未提交的修改永久丢失。习惯用 `git stash` 暂存，用 `git revert` 撤销已提交的内容
4. **Windows 换行符陷阱**：`core.autocrlf=true` 在 checkout 时把 LF→CRLF，commit 时 CRLF→LF。如果配置不一致，`git diff` 每行都显示变化（全是被改变的行尾符）。本仓库应在 `.gitattributes` 中用 `* text=auto` 让 Git 自动检测
5. **本机 Git Bash 下 make 不可用**：直接 `gcc` 编译；`make git-log` 等 target 用 `command -v git` 检测降级
6. **`.gitignore` 不作用于已跟踪文件**：如果一个文件已经 `git add` 过，再加 `.gitignore` 规则不会让它消失——必须先 `git rm --cached <file>` 取消跟踪

## 工程对照（≥100 字硬约束）

Git 是工程化中最"无形"但最深刻的基础设施。它渗透在本仓库的每个层面：

1. **CI/CD pipeline**：`engineering/.github/workflows/` 下的 GitHub Actions 工作流用 `actions/checkout@v4` 拉取仓库，用 `git log` 生成 changelog，用 `git tag` 触发发布——CI 的起点和终点都是 Git
2. **OPSX 变更管理 = Git 工作流的文档化替代**：`openspec/` 下的每个变更目录（proposal.md / tasks.md / specs/）本质上是一个"特性分支"的文档化版本——proposal 是分支的 commit message，tasks 是 commit 清单，spec 是 merge 后的最终状态。不需要真的创建 Git 分支，但思维模型完全一致
3. **`.clang-format` 的配置溯源**：`.clang-format` 在仓库根目录——通过 `git blame .clang-format` 可以追溯到谁在何时引入了哪条格式规则。`.gitattributes` 控制换行符行为，防止不同 OS 的开发者互相污染 diff
4. **`git bisect` 在工程中的实战价值**：本仓库的 `engineering/src/db/api/handlers.c` 有 9 个 REST API handler——如果某个 API 返回码突然从 200 变成 500，`git bisect run ./test_api.sh` 能自动定位到引入 bug 的 commit，比阅读最近 50 个 commit 的 diff 快一个数量级
5. **Git submodule 的成本与收益**：本仓库 `reference/open-source/` 下有 12 个 git submodule（faiss/redis/postgres 等）——submodule 记录了"这个外部依赖的哪个版本能和我现在的代码兼容"，这是 Monorepo 之外另一种依赖管理方案

学完本卡后能动手的事：用 `git bisect` 在 `engineering/src/db/api/handlers.c` 的历史中定位一个假设的回归 bug——写一个 `test_api.sh`，让它返回 0 或 1，然后 `git bisect run` 自动二分。
