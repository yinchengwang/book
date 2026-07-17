# Claude Code 安装完全指南（Windows + Linux + MacOS）

> 综合整理自 CSDN 多篇 Claude Code 安装教程，覆盖国内用户的多种安装方式、API 配置及常见问题。

---

## 一、认识 Claude Code

Claude Code 是 Anthropic 公司开发的官方 AI 编程助手工具，以命令行（CLI）为核心，将 Claude AI 的能力直接集成到开发环境中。

**核心特点：**

- 原生命令行工具，直接在终端中运行
- 理解整个代码库的上下文，支持跨文件编辑
- 支持终端、VS Code、JetBrains IDE 等多种使用环境

**与网页版 Claude 的区别：**

- 网页版需要手动复制粘贴代码，无法直接操作文件
- Claude Code 直接读写项目文件，自动理解项目结构，执行命令

**技术架构（三层）：**

| 层级 | 说明 |
|------|------|
| 交互层 | 终端命令行为核心，IDE 插件为延伸 |
| 中间层 | 智能调度系统：上下文管理、任务拆解、结果适配 |
| 模型层 | 默认对接 Claude 系列模型，也支持第三方兼容模型 |

---

## 二、安装前准备

### 2.1 系统要求

| 系统 | 要求 |
|------|------|
| Windows | Windows 10 1809+ 或 Windows 11，4GB+ 内存，需安装 Git for Windows |
| macOS | macOS 10.15+ |
| Linux | Ubuntu 20.04+ / Debian 10+ |
| Node.js | 如使用 npm 方式安装，需 ≥ 18.0 |

### 2.2 网络要求

- **官方安装**需要访问 `claude.ai` 和 `anthropic.com` 相关域名
- Claude Code 官方安装脚本 `https://claude.ai/install.cmd`（CMD）和 `https://claude.ai/install.ps1`（PowerShell）在国内被封锁
- 国内用户推荐使用 WinGet 或 WSL 方式安装（无需跨外网），或使用国内镜像站

### 2.3 账户要求

Claude Code 本身是免费的命令行工具，但需要调用大语言模型 API 才能工作：

| 账户类型 | 费用 | 说明 |
|----------|------|------|
| Claude Pro | $20/月 | 个人开发者首选 |
| Claude Max | 按量或订阅 | 高用量用户 |
| Anthropic API | 预付费 | 按 token 计费，适合开发团队 |

> 国内用户也可使用 DeepSeek 等国产模型替代，详见第六章。

---

## 三、安装方式选择

根据网络条件选择：

| 你的情况 | 推荐安装方式 |
|----------|------------|
| 国内无跨外网 | WinGet 安装 或 WSL 安装 |
| 有跨外网条件 | CMD 安装 或 PowerShell 安装 |
| macOS/Linux | 官方脚本安装 |

---

## 四、Windows 安装

### 方式一：WinGet 安装（推荐国内用户 ✅）

**优点：** 无需跨外网，无需前置设置，从 Microsoft 官方仓库安装
**缺点：** 不支持自动更新，需要手动更新

```powershell
# 安装
winget install Anthropic.ClaudeCode

# 更新（需定期手动执行）
winget upgrade Anthropic.ClaudeCode

# 卸载
winget uninstall Anthropic.ClaudeCode
```

安装成功后，`claude.exe` 位于：
```
C:\Users\<用户名>\AppData\Local\Microsoft\WinGet\Packages\Anthropic.ClaudeCode_...
```

### 方式二：CMD 安装（需跨外网 ✅）

**优点：** 无需前置设置，一条命令搞定，支持自动更新
**缺点：** 需要访问 `https://claude.ai`（国内被封）

```cmd
# 安装
curl -fsSL https://claude.ai/install.cmd -o install.cmd && install.cmd && del install.cmd

# 手动更新（通常不需要，会自动更新）
claude update

# 卸载
del /F "%USERPROFILE%\.local\bin\claude.exe"
rmdir /S /Q "%USERPROFILE%\.local\share\claude"
rmdir /S /Q "%USERPROFILE%\.claude" 2>nul
```

安装位置：`C:\Users\<用户名>\.local\bin`

**常见问题：SSL 证书验证失败**
```
curl: (60) schannel: SNI or certificate check failed
```
解决方法：检查系统时间是否正确，尝试使用 PowerShell 安装，或改用 WinGet 安装。

### 方式三：PowerShell 安装（需跨外网）

**步骤 1：** 设置执行策略（首次安装需要）

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
# 出现提示时输入 Y 确认
```

**步骤 2：** 执行安装

```powershell
# 安装
Invoke-RestMethod -Uri https://claude.ai/install.ps1 | Invoke-Expression

# 手动更新
claude update
```

> `-Scope CurrentUser` 表示只影响当前用户，不需要管理员权限。

### 方式四：npm 安装

**前提：** Node.js 版本 ≥ 18.0.0

```bash
# 检查 Node 版本
node --version   # 必须 v18.x 或更高

# 设置国内镜像源（加速下载）
npm config set registry https://registry.npmmirror.com

# 安装最新版本
npm install -g @anthropic-ai/claude-code

# 卸载
npm uninstall -g @anthropic-ai/claude-code
```

---

## 五、WSL 安装

适用于想在 Windows 上使用 Linux 环境的用户，支持自动更新。

### 5.1 安装 WSL 环境

```bash
# 一键安装 WSL（会自动安装 Ubuntu 并设置 WSL2 为默认）
wsl --install
```

或者手动步骤：

1. 控制面板 → 程序 → 启用或关闭 Windows 功能 → 勾选"适用于 Linux 的 Windows 子系统"和"虚拟机平台"
2. 重启电脑后，管理员终端运行：

```bash
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart
```

3. 更新 WSL2 内核并设为默认：

```bash
wsl --update
wsl --set-default-version 2
```

4. 检查发行版状态：

```bash
wsl --list --verbose
```

### 5.2 在 WSL 中安装 Claude Code

打开 WSL 终端：

```bash
# 安装 Claude Code
curl -fsSL https://claude.ai/install.sh | bash

# 手动更新（通常不需要）
claude update

# 卸载
rm -f ~/.local/bin/claude && rm -rf ~/.local/share/claude
```

> WSL 1 和 WSL 2 都支持，推荐使用 WSL 2。

---

## 六、macOS / Linux 安装

### 官方脚本安装

```bash
# macOS / Linux / WSL
curl -fsSL https://claude.ai/install.sh | bash

# 或使用国内镜像
source <(curl -fsSL https://claude-zh.cn/scripts/install.sh)
```

### Homebrew 安装（macOS）

```bash
brew install --cask claude-code

# 卸载
brew uninstall --cask claude-code
```

### npm 安装

```bash
npm install -g @anthropic-ai/claude-code
```

---

## 七、验证安装

```bash
# 检查版本
claude --version

# 查看安装路径
where claude     # Windows
which claude     # macOS/Linux

# 运行健康检查
claude doctor
```

如果输出版本号（如 `2.1.92 (Claude Code)`），说明安装成功。

**如果提示"找不到命令"：**
- **重启终端**（关闭重开，不是刷新）
- macOS/Linux：`export PATH="$HOME/.claude/bin:$PATH"` 并写入 `~/.zshrc`
- Windows：在系统环境变量 Path 中添加 `%USERPROFILE%\.claude\bin`

---

## 八、配置 settings.json

创建或编辑 `~/.claude/settings.json`（Windows 在 `C:\Users\<用户名>\.claude\settings.json`）：

```json
{
  "hasCompletedOnboarding": true,
  "env": {
    "CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC": "1"
  },
  "includeCoAuthoredBy": false
}
```

---

## 九、API 配置（国内用户重点）

Claude Code 原生的 Claude 大模型在国内无法直接使用，需要配置替代方案。

### 方案一：使用官方 API（需跨外网）

在 `~/.claude/settings.json` 中配置：

```json
{
  "env": {
    "ANTHROPIC_BASE_URL": "https://api.anthropic.com",
    "ANTHROPIC_AUTH_TOKEN": "你的API密钥"
  }
}
```

### 方案二：使用第三方中转/镜像

使用 `cc-switch` 工具（推荐）：

```bash
# 安装 cc-switch
npm install -g cc-switch

# 选择可用的镜像站
cc-switch
```

### 方案三：使用 DeepSeek API（国内推荐）

访问 [DeepSeek 官网](https://www.deepseek.com/) → API 开放平台 → 创建 API Key。

在 `settings.json` 中配置：

```json
{
  "env": {
    "ANTHROPIC_BASE_URL": "https://api.deepseek.com/anthropic",
    "ANTHROPIC_AUTH_TOKEN": "你的DeepSeek API Key",
    "ANTHROPIC_MODEL": "deepseek-reasoner",
    "ANTHROPIC_DEFAULT_SONNET_MODEL": "deepseek-chat",
    "ANTHROPIC_DEFAULT_OPUS_MODEL": "deepseek-reasoner",
    "ANTHROPIC_DEFAULT_HAIKU_MODEL": "deepseek-chat",
    "ANTHROPIC_MAX_TOKENS": "6000",
    "ANTHROPIC_TEMPERATURE": "0.2",
    "API_TIMEOUT_MS": "600000",
    "CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC": "1"
  }
}
```

> `ANTHROPIC_TEMPERATURE`：代码/数学推理建议 0.1~0.3，文学创作可调到 0.8 以上。

### 方案四：通过环境变量配置（Windows）

设置系统环境变量：

| 变量名 | 变量值示例 |
|--------|-----------|
| `ANTHROPIC_BASE_URL` | `https://api.deepseek.com/anthropic` |
| `ANTHROPIC_API_KEY` | 你的密钥 |
| `ANTHROPIC_AUTH_TOKEN` | 你的密钥 |

---

## 十、启动与使用

```bash
# 进入项目目录后启动
cd your-project
claude

# 或指定模型启动
claude --model deepseek-chat
```

出现 `Welcome to Claude Code` 提示和交互式提示符 `>` 即为成功。

### 核心命令

| 命令 | 说明 |
|------|------|
| `claude` | 启动交互模式 |
| `claude --version` | 查看版本 |
| `claude doctor` | 健康检查 |
| `claude update` | 手动更新 |
| `/model` | 在交互中切换模型 |

---

## 十一、IDE 集成

### VS Code
安装 Claude Code 扩展后，在 VS Code 终端中直接使用 `claude` 命令。

### JetBrains IDE
安装 Claude Code 插件，通过 IDE 面板或内置终端使用。

---

## 十二、自动更新说明

- CMD/PowerShell/macOS/Linux 官方脚本安装：**默认自动更新**
- WinGet 安装：**不支持自动更新**，需手动执行 `winget upgrade Anthropic.ClaudeCode`

### 禁用自动更新

```bash
# 在 settings.json 的 env 中添加
"CLAUDE_CODE_AUTO_UPDATE": "0"
```

---

## 十三、常见问题

| 错误现象 | 可能原因 | 解决方案 |
|----------|---------|---------|
| `curl: command not found` | 系统没有 curl | 安装 curl |
| `npm ERR! code EACCES` | npm 全局权限不足 | 按官方方案修复 npm 权限 |
| `claude: command not found` | PATH 未更新 | 重启终端，或手动添加 PATH |
| `Authentication failed` | 账户/API 问题 | 检查 API Key 是否正确或余额是否充足 |
| SSL 证书验证失败 | 代理/防火墙问题 | 检查系统时间，或换 WinGet 安装 |
| 安装后执行显示异常 | 代理设置问题 | 确保代理节点为 US，出站模式为全局，开启 TUN 模式 |

---

## 十四、卸载方法汇总

| 安装方式 | 卸载命令 |
|----------|---------|
| WinGet | `winget uninstall Anthropic.ClaudeCode` |
| CMD | 删除 `%USERPROFILE%\.local\bin\claude.exe` 和 `%USERPROFILE%\.local\share\claude` |
| PowerShell | 删除 `%USERPROFILE%\.claude` 目录 |
| npm | `npm uninstall -g @anthropic-ai/claude-code` |
| macOS/Linux | `rm -rf ~/.claude` |
| Homebrew | `brew uninstall --cask claude-code` |
| WSL | `rm -f ~/.local/bin/claude && rm -rf ~/.local/share/claude` |

> 重新安装后，`~/.claude/settings.json` 会保留，无需重新配置。

---

## 参考资料

- [Claude Code 官方文档](https://docs.anthropic.com/en/docs/claude-code/overview)
- [Claude Code 中文文档](https://docs.anthropic.com/zh-CN)
- [Anthropic 控制台](https://console.anthropic.com)
- [Claude Code GitHub 仓库](https://github.com/anthropics/claude-code)
- [Claude Code 中文站](https://claude-zh.cn/guide/getting-started)
- [DeepSeek API 平台](https://www.deepseek.com/)
