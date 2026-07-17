# OpenSpec 完整实战指南：从安装初始化到命令体系与闭环落地

> 本文基于多篇公开资料与实践经验综合整理，目标是给你一篇能直接上手、又能覆盖进阶命令体系的 OpenSpec 中文指南。

---

## 一、为什么需要 OpenSpec

很多团队在使用 AI 编程助手时，会反复踩这几个坑：

- 需求描述散落在聊天记录里，过几天就找不到上下文
- AI 每次生成代码风格和边界不一致，返工多
- 功能做完后说不清“为什么这么改”

OpenSpec 的核心价值就是把“口头需求”变成“结构化变更资产”，形成可追溯、可验证、可归档的闭环。

一句话：先对齐规格，再让 AI 写代码。

---

## 二、5 分钟入门：安装与初始化

## 2.1 安装 CLI

```bash
# npm
npm install -g @fission-ai/openspec@latest

# 或 pnpm
pnpm install -g @fission-ai/openspec@latest
```

验证安装：

```bash
openspec --version
```

## 2.2 初始化项目

在项目根目录执行：

```bash
# 仅配置某个工具（示例）
openspec init --tools cursor

# 配置所有支持工具
openspec init --tools all

# 更新已有项目中的 OpenSpec 指令与命令
openspec update
```

如果你在 IDE 中使用斜杠命令，初始化或 update 后通常需要重启 IDE，让命令加载生效。

## 2.3 初始化后目录结构

```text
openspec/
├── specs/                     # 主规范：当前系统“已生效”的行为
├── changes/                   # 变更工作区：每个 change 一个文件夹
│   └── <change-name>/
│       ├── .openspec.yaml
│       ├── proposal.md
│       ├── specs/
│       ├── design.md
│       └── tasks.md
└── config.yaml                # 可选：项目上下文与规则
```

可以把它理解成两层：

- specs：系统事实（当前状态）
- changes：变更草案与执行过程（未来状态）

---

## 三、核心概念：主规范、增量规范、工件链

## 3.1 主规范 vs 增量规范

- 主规范（Main Specs）：放在 `openspec/specs/`，记录系统当前正式行为
- 增量规范（Delta Specs）：放在 `openspec/changes/<name>/specs/`，记录本次改动

当 change 完成并归档后，增量规范会合并回主规范。

## 3.2 工件依赖链

默认 spec-driven 工作流中，常见依赖是：

```text
proposal -> specs -> design -> tasks -> implement -> verify -> archive
```

实操里不是“死板阶段”，而是“可回改工件的动作流”：在实现过程中发现问题，可以回头更新 proposal/specs/design/tasks。

---

## 四、命令全景：Core 与 Expanded

说明：不同 AI 工具里命令前缀可能有差异（如 `/opsx:xxx`、`/opsx-xxx`、`/openspec-xxx`），但语义一致。

## 4.1 Core（默认快速工作流）

| 命令 | 用途 | 适用时机 |
|---|---|---|
| `/opsx:propose` | 一步创建 change 并生成规划工件 | 需求明确，想快速开工 |
| `/opsx:explore` | 探索问题、比较方案、澄清需求 | 需求模糊或技术路线未定 |
| `/opsx:apply` | 按 tasks 实现代码并推进任务勾选 | 进入开发执行阶段 |
| `/opsx:archive` | 完成变更并归档，保留审计历史 | 实现收尾 |

## 4.2 Expanded（复杂场景增强）

| 命令 | 用途 | 适用时机 |
|---|---|---|
| `/opsx:new` | 只创建 change 脚手架 | 想精细控制工件生成 |
| `/opsx:continue` | 逐步创建下一个可用工件 | 需要“每一步都审查” |
| `/opsx:ff` | 快进生成全部规划工件 | 范围清晰，中小变更 |
| `/opsx:verify` | 校验实现与工件一致性 | 归档前必做检查 |
| `/opsx:sync` | 提前把 Delta 合并回主规范 | 长周期/并行 change |
| `/opsx:bulk-archive` | 批量归档多个完成变更 | 团队并行开发收口 |
| `/opsx:onboard` | 引导式教程 | 新手第一次接入 |

## 4.3 如何选命令（决策速查）

- 需求明确、追求速度：`propose -> apply -> verify -> archive`
- 需求复杂、要分步审查：`new -> continue(多次) -> apply -> verify -> archive`
- 需求模糊：先 `explore`，再进入上述任一流程
- 并行多变更收尾：`verify x N -> bulk-archive`

---

## 五、实战闭环：以 add-dark-mode 为例

下面给一个典型闭环，适合你复制到真实项目中。

## 5.1 创建变更

快速模式：

```text
/opsx:propose add-dark-mode
```

或精细模式：

```text
/opsx:new add-dark-mode
/opsx:continue
/opsx:continue
...
```

## 5.2 检查产物

预期至少有：

- `proposal.md`：为什么做、改什么、影响范围
- `specs/*/spec.md`：新增/修改/删除需求 + 场景
- `design.md`：技术方案、关键取舍
- `tasks.md`：可执行任务清单

## 5.3 执行实现

```text
/opsx:apply add-dark-mode
```

执行期建议：

- 任务粒度保持在 1-2 小时内可完成
- 每完成一项就更新勾选状态，避免“看起来做完，实际没落地”

## 5.4 归档前验证

```text
/opsx:verify add-dark-mode
```

重点看三类结果：

- 完整性：任务是否完成、需求是否落地、场景是否覆盖
- 正确性：实现是否符合规格意图
- 一致性：代码是否体现 design 决策

如果出现 warning，不一定阻塞归档，但建议先修复高风险项。

## 5.5 同步并归档

```text
/opsx:archive add-dark-mode
```

如果系统提示 Delta 尚未同步，可按提示同步后归档。最终变更会进入归档目录，形成可追溯历史。

---

## 六、常见问题与避坑

## 6.1 Change not found

可能原因：上下文无法推断当前 change。

处理方式：

- 命令里显式带 change 名
- 用 `openspec list` 确认变更是否存在
- 确认当前目录是正确项目根目录

## 6.2 No artifacts ready

可能原因：依赖未满足或工件已全部生成。

处理方式：

- 查看当前 change 状态
- 先补齐前置工件，再继续 `continue`

## 6.3 命令在 IDE 中不生效

可能原因：初始化后命令未重载。

处理方式：

- 运行 `openspec update`
- 重启 IDE
- 检查工具目录下命令/技能是否已生成

## 6.4 制品质量不稳定

常见原因：项目上下文不足、描述过于抽象。

处理方式：

- 在 `openspec/config.yaml` 增补项目上下文
- 为 proposal/tasks 增加规则约束
- 对复杂变更改用 `continue` 分步生成

---

## 七、最佳实践（给团队落地）

- 一个 change 只做一件事，避免“功能 + 大重构”混在一起
- change 名使用可读 kebab-case，如 `fix-login-redirect`
- 归档前固定执行 `verify`
- 长周期变更按需 `sync`，减少并行冲突
- 把 OpenSpec 与 Git 分支命名对应起来，便于追踪

---

## 八、新手最小闭环（可直接照做）

```text
1) openspec init
2) /opsx:propose add-xxx
3) /opsx:apply
4) /opsx:verify
5) /opsx:archive
```

这 5 步走通后，再根据项目复杂度切换到 `new/continue/ff` 等扩展命令。
