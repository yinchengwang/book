
# 一、入门

## 1.1 claude code安装
- [Claude Code 安装](https://blog.csdn.net/weixin_48232848/article/details/160961579?spm=1001.2014.3001.5501)

## 1.2 创建 CLAUDE.md
方式一：使用 /init 命令自动生成（推荐）

执行位置：先 cd 到你的项目根目录，然后启动 claude 并输入 /init

```
PS C:\code\book> claude
 ▐▛███▜▌   Claude Code v2.1.141
▝▜█████▛▘  deepseek-v4-pro with high effort · API Usage Billing
  ▘▘ ▝▝    C:\code\book
❯ /init                                                                                                                                                                             

● 我来分析现有文档并检查代码库的实际状态，然后提出改进建议。
```

创建方式二：手动创建
	在项目根目录下创建文件 `CLAUDE.md`，
**文件位置优先级：**

| 优先级 | 路径                | 位置                                | 说明                                               |
| ------ | ------------------- | ----------------------------------- | -------------------------------------------------- |
| 1      | ./CLAUDE.md         | project\CLAUDE.md                   | 项目根目录，独属于当前的项目（推荐，可提交到 git） |
| 2      | ./CLAUDE.local.md   | project\CLAUDE.local.md             | 项目本地（不提交到 git）                           |
| 3      | ~/.claude/CLAUDE.md | C:/User/username/. claude/CLAUDE.md | 全局配置, 作用于所有的项目                         |
| 4      | 父目录              | 自动向上查找                        |                                                    |

**官方示例：**

```markdown
# 项目说明
这是一个 TypeScript + React 项目，使用 Vite 构建。

# 代码规范
- 使用 ESLint 和 Prettier
- 组件使用函数式写法
- 测试文件放在 __tests__ 目录

# 常用命令
- npm run dev: 启动开发服务器
- npm run test: 运行测试
- npm run build: 构建生产版本

# 重要提示 这些强调词可以提高遵循度
IMPORTANT: 所有 API 请求必须经过 src/api/request.ts 封装
YOU MUST: 新增组件需要同时编写单元测试
```

## 1.3 配置工具权限

Claude Code 默认采用保守的权限策略，可以通过以下四种方式显式授权：

| 方式                | 说明                             |
| ------------------- | -------------------------------- |
| 交互式授权          | 启动时的授权提示，逐个确认       |
| `/permissions` 命令 | 在会话中管理权限                 |
| 编辑配置文件        | 手动编辑 `.claude/settings.json` |
| 启动参数            | `--allowedTools` 参数            |

**方式一：在 Claude Code 交互界面中输入 `/permissions`**

```claude
❯ /permissions                                                                                                                                     

───────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────
  Permissions  Recently denied   Allow   Ask   Deny   Workspace              

  Claude Code won't ask before using allowed tools.
  ╭────────────────────────────────────────────────────────────────────────────────────────────────╮
  │ ⌕ Search…                                                                                      │
  ╰────────────────────────────────────────────────────────────────────────────────────────────────╯

    1.  Add a new rule…
```

**方式二：启动时通过命令行参数授权**

```
C:\code\book> claude --allowedTools Edit,xxxx
```

## 1.4 开启免授权模式

频繁的授权提示会影响效率，可以开启免授权模式：

```
C:\code\book> claude --dangerously-skip-permissions

⚠️  WARNING: You are about to enable Bypassing Permissions mode
This will allow Claude to execute commands without confirmation

Are you sure? (Yes/No): Yes

╭─────────────────────────────────────────────────────────────╮
│  Claude Code  [Bypassing Permissions]                       │
╰─────────────────────────────────────────────────────────────╯
>
```

启动后终端会显示黄色的 `Bypassing Permissions` 标识。

**设置别名永久生效：**

Windows PowerShell —— 配置文件位置 `C:\Users\<用户名>\Documents\PowerShell\Microsoft.PowerShell_profile.ps1`：

```powershell
# 打开 PowerShell 配置文件
PS C:\> notepad $PROFILE

# 在文件中添加以下内容：
function cc { claude --dangerously-skip-permissions $args }

# 保存后重新加载配置
PS C:\> . $PROFILE

# 之后就可以用 cc 命令启动了
PS C:\> cc
```

Mac/Linux —— 配置文件位置 `~/.bashrc` 或 `~/.zshrc`：

```bash
# 添加别名到配置文件
$ echo "alias cc='claude --dangerously-skip-permissions'" >> ~/.zshrc

# 重新加载配置
$ source ~/.zshrc
```

> ⚠️ **安全警告：** 官方建议在没有网络访问的容器中使用此模式，以避免 prompt injection 等数据泄露风险。

# 二、高效提需求

## 2.1 先理解项目再动手

在修改代码之前，先让 Claude 理解你的项目，可以这样提问：

```
分析一下数据库表结构
这个应用中的错误是如何处理的？
认证模块是怎么实现的？
支付流程是怎么工作的？
```

核心思想：修改前让 AI 理解业务和代码，才能更精准、高效地辅助开发。

## 2.2 需求描述要具体

| 质量 | 示例 |
|------|------|
| ❌ 差 | 修复这个漏洞 |
| ✅ 好 | 修复用户登录时不输入密码出现的空指针错误 |
| ❌ 差 | 给 foo.py 添加测试 |
| ✅ 好 | 给 foo.py 编写一个新的测试用例，覆盖用户已登出的边界情况，不要使用 mock |
| ❌ 差 | 添加一个日历组件 |
| ✅ 好 | 先看看首页现有的组件是怎么实现的，理解一下代码模式。xxxx 是个好例子。然后按照这个模式实现一个新的日历组件，让用户可以选择月份，并且可以前后翻页选择年份 |

## 2.3 复杂需求分步执行

小任务可以一次性发送，AI 能整体考虑代码结构和风格。大需求建议拆解成小步骤：

```
第一步：给用户 API 创建一个新接口
第二步：给请求的字段添加必要的验证
第三步：编写这个接口的测试用例
第四步：更新 API 文档
```

原因：
- AI 上下文有限制，代码量太长可能输出不全或被截断
- 分步执行更安全，每一步完成后可以先 review/测试

# 三、官方推荐工作流

## 3.1 探索 → 计划 → 编码 → 提交

这是 Anthropic 官方推荐的通用工作流：

**第一步：探索**（明确告诉它不要写代码）

```
请阅读 src/auth 目录下的文件，理解认证模块的实现方式，但先不要写代码
```

**第二步：计划**（使用 think 关键词）
```
请 think hard 制定一个实现用户权限管理的方案
```

**第三步：编码**
```
请按照刚才的方案实现代码，并验证实现的合理性
```

**第四步：提交**
```
请提交代码并创建 Pull Request，同时更新 README
```

> 💡 步骤 1-2 非常重要！没有它们，Claude 容易直接跳到编码阶段。

## 3.2 测试驱动开发（TDD）

Anthropic 内部最喜欢的工作流之一：

```
1. 请根据预期的输入输出编写测试用例
   （明确告诉它我们在做 TDD，避免创建 mock 实现）

2. 请运行测试并确认它们失败，不要写实现代码

3. 测试满意后，请提交测试

4. 请编写代码让测试通过，不要修改测试
   继续直到所有测试通过

5. 测试全部通过后，请提交代码
```

## 3.3 视觉驱动开发

适用于 UI 开发：

```
1. 给 Claude 提供截图能力（如 Puppeteer MCP 服务器）
2. 拖拽或粘贴设计稿图片
3. 请实现这个设计，截图查看结果，然后迭代直到匹配设计稿
4. 满意后请提交
```

# 四、深度思考模式

使用 "think" 关键词激活深度思考模式，官方确认有四个级别：

| 级别 | 关键词 | 思考预算 | 适用场景 |
|------|--------|----------|----------|
| 1 | think | 基础 | 简单问题 |
| 2 | think hard | 中等 | 中等复杂度 |
| 3 | think harder | 深入 | 复杂问题 |
| 4 | ultrathink | 最大 | 最复杂的架构/算法问题 |

使用示例：

```
请 think hard 分析这段代码的性能问题

请 ultrathink 设计一个高并发的消息队列架构
```

官方说明：这些关键词直接映射到系统中不同级别的思考预算，每一级都会逐步增加 Claude 可用的思考预算。

**费用提醒：**
- `ultrathink` 消耗最大，一个简单的 1+1 计算可能耗费 0.06 美元
- Max 套餐用户可以放心使用，Pro 用户需注意用量

# 五、实操技巧

## 5.1 发送图片处理

Claude Code 支持发送图片进行处理：

- **粘贴方式：** Mac 使用 `Ctrl + V`（不是 Command + V），Windows/Linux 使用 `Ctrl + V`

使用场景：

```
这个图片显示了什么？
这是错误的截图，是什么原因导致的？
请根据这个图片的设计稿实现网页
```

## 5.2 恢复历史会话

**非交互模式下**（还没进入 Claude Code）：

```
# 自动继续最近的对话
C:\code\book> claude --continue
# 或简写
C:\code\book> claude -c

# 显示历史对话选择器
C:\code\book> claude --resume
# 或简写
C:\code\book> claude -r

Select a session to resume:
> 2025-01-08 14:30 - 修复登录bug
  2025-01-08 10:15 - 添加用户模块
  2025-01-07 16:45 - 重构API
```

| 命令 | 说明 |
|------|------|
| `claude --continue` 或 `claude -c` | 自动继续最近的对话 |
| `claude --resume` 或 `claude -r` | 显示历史对话选择器 |

**交互模式下**（已进入 Claude Code）：

在交互界面中输入 `/resume`，使用上下方向键选中历史记录即可恢复。

## 5.3 编辑记忆文件

在 Claude Code 交互界面中输入 `/memory`，记忆文件位置：

- 用户级：`C:\Users\<用户名>\.claude\CLAUDE.md`
- 项目级：`<项目根目录>\CLAUDE.md`

示例——设置永远用中文回复，在用户级记忆文件中添加：

```
每次请用中文回答我。
```

设置后，所有项目的交互都会使用中文回答。

## 5.4 上下文管理

`/clear` vs `/compact` 的区别：

| 命令 | 作用 | 使用场景 |
|------|------|----------|
| `/clear` | 完全清除对话历史 | 切换到完全不同的任务 |
| `/compact` | 压缩对话但保留摘要 | 继续当前任务但需要释放空间 |

`/compact` 高级用法：

```
> /compact focus on auth errors from the last two commits

✓ Conversation compacted
✓ Preserved context: auth errors from recent commits
✓ Tokens reduced: 45,000 → 8,500
```

可以指定保留重点，让摘要更有针对性。

**管理建议：**
- 使用 `/context` 查看当前上下文使用情况
- Claude Code 默认在上下文超过 95% 容量时自动压缩
- 可通过 `/config` 开启/关闭自动压缩
- 建议在 70% 容量时主动压缩

**有效管理成本和性能：**
- 定期使用 `/compact` 手动压缩
- 定时使用 `/clear` 重置上下文
- 分解复杂任务，需求尽量具体化

# 六、斜杠命令完整参考

## 6.1 核心命令

| 命令 | 功能说明 |
|------|----------|
| `/help` | 显示帮助和可用命令 |
| `/init` | 初始化项目，生成 CLAUDE.md |
| `/clear` | 清除对话历史 |
| `/compact [instructions]` | 压缩对话，可指定保留重点 |
| `/exit` | 退出 Claude Code |

## 6.2 会话管理

| 命令 | 功能说明 |
|------|----------|
| `/resume [session]` | 恢复历史会话（按 ID 或名称） |
| `/rename <name>` | 重命名当前会话 |
| `/export [filename]` | 导出对话到文件或剪贴板 |

## 6.3 模型与费用

| 命令 | 功能说明 |
|------|----------|
| `/model` | 选择或切换 AI 模型 |
| `/cost` | 显示 Token 使用统计 |
| `/usage` | 显示订阅计划用量和限制（仅订阅用户） |
| `/stats` | 可视化每日使用量、会话历史等 |

## 6.4 配置与状态

| 命令 | 功能说明 |
|------|----------|
| `/config` | 打开设置界面 |
| `/status` | 显示版本、模型、账户和连接状态 |
| `/permissions` | 查看或更新权限 |
| `/memory` | 编辑 CLAUDE.md 记忆文件 |
| `/doctor` | 检查 Claude Code 安装健康状态 |
| `/context` | 可视化当前上下文使用情况 |

## 6.5 开发工具

| 命令 | 功能说明 |
|------|----------|
| `/review` | 请求代码审查 |
| `/security-review` | 完成当前分支的安全审查 |
| `/add-dir` | 添加额外的工作目录 |
| `/mcp` | 管理 MCP 服务器连接 |
| `/hooks` | 管理工具事件的钩子配置 |
| `/todos` | 列出当前 TODO 项目 |
| `/pr-comments` | 查看 Pull Request 评论 |

# 七、与工具系统交互

## 7.1 Git 交互

用自然语言操作 Git，无需记忆复杂命令：

| 需求 | 自然语言 |
|------|----------|
| 查看修改 | 我修改了哪些文件 |
| 智能提交 | 用合理描述性信息提交我的更改 |
| 推送代码 | 推送本分支到远程 |
| 切换分支 | 删除本分支并切换到 master 分支 |
| 查看历史 | 显示最近 3 次提交中所有文件列表 |
| 创建 PR | 为这些更改创建一个 Pull Request |
| 解决冲突 | 帮我解决合并冲突 |

## 7.2 GitHub 交互

如果安装了 `gh` CLI，Claude 可以：

```
# 创建 Issue
为这个 bug 创建一个 GitHub Issue

# 查看 PR 评论
/pr-comments

# 安装 GitHub App
/install-github-app
```

## 7.3 Linux/Shell 命令助手

**交互模式：** 在 Claude Code 交互界面中直接用自然语言描述需求，Claude 会自动生成并执行复杂的命令（如 find、wc、sort 组合）。

**非交互模式：** 使用 `-p` 参数执行单次命令，执行完成后自动退出：

```
$ claude -p "列出行数最多的前3个.java文件"
```

## 7.4 代码库问答

新人入职时的利器：

```
这个项目的认证是怎么工作的？
数据库 schema 在哪里定义的？
如何添加一个新的 API 端点？
支付流程是怎么实现的？
```

Anthropic 内部已将此作为核心入职工作流，显著提升了新人上手速度。

# 八、模型切换与费用

## 8.1 模型切换

Claude Code 支持 Claude Opus 与 Claude Sonnet 4 两个模型：

```
/model
```

| 模型 | 特点 | 适用用户 |
|------|------|----------|
| Claude Sonnet 4 | 默认模型，性价比高 | Pro / Max |
| Claude Opus | 更强大，费用是 Sonnet 的 5 倍 | 仅 Max |

> 💡 建议：日常使用 Sonnet 4 即可，体验与 Opus 差别不大，但费用仅为 1/5。

## 8.2 查看消耗

**方式一：内置命令**

在交互界面中输入 `/cost`，显示当前会话已消耗的 Token 和费用。

**方式二：ccusage 工具（推荐）**

```
# 安装 ccusage
$ npm install -g ccusage

# 查看指定日期后的消耗
$ ccusage -s 20250701

Usage from 2025-07-01:
├─ Total sessions: 45
├─ Total tokens: 1,234,567
└─ Total cost: $45.67

# 实时查看消耗
$ ccusage blocks --live
```

## 8.3 订阅说明

| 订阅类型 | 计费方式 | 模型支持 | 说明 |
|----------|----------|----------|------|
| Pro | 按月计费 | 仅 Sonnet | 超量后暂时不可用，等待恢复 |
| Max | 按月计费 | Sonnet + Opus | 超量后暂时不可用，等待恢复 |

Pro/Max 订阅用户是按月计费，超过使用量后需要等待指定时间恢复。

# 九、自定义命令

## 9.1 命令类型

| 类型 | 目录位置 | 调用方式 | 作用范围 |
|------|----------|----------|----------|
| 项目级 | `.claude/commands/` | `/project:<name>` | 仅当前项目 |
| 用户级 | `~/.claude/commands/` | `/user:<name>` | 所有项目 |

Windows 实际路径：
- 项目级：`<项目>\.claude\commands\`
- 用户级：`C:\Users\<用户名>\.claude\commands\`

注意：如果项目命令和用户命令同名，项目命令优先。

## 9.2 创建项目级命令

```powershell
# 进入项目目录
PS C:\> cd C:\code\book

# 创建命令目录
PS C:\code\book> mkdir -p .claude\commands

# 创建命令文件
PS C:\code\book> New-Item -Path ".claude\commands\optimize.md" -Value "分析这个项目的性能，并提出三个具体的优化建议。"
```

使用方式（在 Claude Code 中）：`/project:optimize`

## 9.3 创建用户级命令

```powershell
# 创建用户级命令目录
PS C:\> mkdir -p $HOME\.claude\commands

# 创建推送命令文件
PS C:\> New-Item -Path "$HOME\.claude\commands\push.md" -Value "用合理描述性信息提交所有变更文件，然后推送到远程仓库。"
```

使用方式（在 Claude Code 中）：`/user:push`

## 9.4 高级功能

**使用参数（`$ARGUMENTS`）：**

`.claude/commands/fix-issue.md`：

```markdown
请修复 GitHub Issue #$ARGUMENTS

1. 先阅读 Issue 内容
2. 分析问题原因
3. 实现修复方案
4. 编写测试验证
```

使用方式：`/project:fix-issue 1234`

**使用位置参数（`$1`, `$2`）：**

`.claude/commands/create-component.md`：

```markdown
请在 $1 目录下创建名为 $2 的组件
```

使用方式：`/project:create-component src/components Button`

**使用 Frontmatter：**

```markdown
---
description: 修复 GitHub Issue
allowed-tools:
  - Edit
  - Bash(git:*)
  - Read
argument-hint: <issue-number>
---

请修复 GitHub Issue #$1
```

**命名空间（子目录）：**

文件位置 `.claude/commands/frontend/component.md`，使用方式 `/project:frontend:component`。

# 十、快捷键速查

| 快捷键 | 功能 |
|--------|------|
| `/` | 查看所有斜杠命令 |
| `↑` / `↓` | 浏览命令历史 |
| `Tab` | 命令/文件路径快速补全 |
| `Shift + Tab` | 切换自动接受模式 |
| `Escape` | 中断 Claude 当前响应（保留已生成内容） |
| `Ctrl + C` | 取消当前输入/退出 |
| `Ctrl + L` | 清屏 |
| `Ctrl + V` | 粘贴图片（Mac 也是 Ctrl+V） |
| `Option + Enter` | 换行（Mac） |

# 附录：多 Claude 工作流

## 一个写代码，一个审查

```
1. 用 Claude A 写代码
2. 运行 /clear 或启动新终端的 Claude B
3. 让 Claude B 审查 Claude A 的代码
4. 再启动 Claude C 根据审查意见修改代码
```

## 使用 Git Worktrees 并行开发

```bash
# 创建 worktree
$ git worktree add ../project-feature-a feature-a
$ git worktree add ../project-feature-b feature-b

# 在每个 worktree 启动 Claude（打开多个终端窗口）
# 终端 1:
$ cd ../project-feature-a && claude

# 终端 2:
$ cd ../project-feature-b && claude
```

优势：比多次 clone 更轻量，共享 Git 历史和 reflog。

# 参考资料

- [Claude Code Best Practices - Anthropic 官方](https://docs.anthropic.com/en/docs/claude-code/best-practices)
- [Claude Code 官方文档](https://docs.anthropic.com/en/docs/claude-code)

