# Obsidian Vault 设置指南

> 面向**外部读者**的 Obsidian Vault 安装与使用指南。本指南不依赖仓库内其他代码，纯粹教你怎么打开 `learning/notes/` 并开始用。

## 适用人群

- 你想用一个 vault 管理 C/C++/数据库/Linux 学习笔记
- 你喜欢双向链接 + Dataview 查询
- 你愿意接受"学习笔记 + 工程代码"分离的双轨架构

## 1. 安装 Obsidian

- 官网：<https://obsidian.md/>
- 下载对应平台（Windows/macOS/Linux）
- **不需要账号**，本地应用即可

## 2. 克隆仓库

```bash
git clone <repo-url> book
cd book
```

## 3. 打开 Vault

1. 启动 Obsidian
2. File → Open vault → Open folder as vault
3. 选择 `book/learning/notes/` 目录
4. 第一次打开 Obsidian 会提示信任插件 → 选"Trust author"

## 4. 启用社区插件

第一次打开会提示启用 community plugins。需要的插件：

| 插件 | 用途 |
|---|---|
| **dataview** | 实时查询笔记（仪表盘必备） |
| **templater** | 模板插入（4 类笔记模板） |
| **obsidian-graph-analysis** | 增强图视图（双链可视化） |
| **obsidian-tag-pages** | 标签聚合页 |
| **backlink** | 反向链接面板 |

这些插件已经在 `learning/notes/.obsidian/community-plugins.json` 注册，Obsidian 会自动提示启用。

如果没提示，手动启用：
1. Settings → Community plugins → 已安装插件
2. 找到每个插件 → 点 Enable

## 5. 浏览 Vault

打开以下页面熟悉结构：

- `_index/home.md` —— 总索引 + 6 轨道入口
- `_dashboard/progress.md` —— Dataview 仪表盘（自动渲染）
- `_meta/schema.md` —— Frontmatter 规范
- `_templates/theory.md` 等 4 类模板

## 6. 写第一篇笔记

### 方法 A：用模板（Templater 插件）

1. Ctrl+P → "Templater: Create new note from template"
2. 选择 `theory` 或 `practice` 等模板
3. 填写 frontmatter
4. 写正文

### 方法 B：手工创建

1. 在 vault 内任意位置右键 → New note
2. 顶部粘贴 frontmatter（参考 `_meta/schema.md`）：
   ```yaml
   ---
   stack: linux
   difficulty: medium
   status: in-progress
   links: []
   created: 2026-XX-XX
   updated: 2026-XX-XX
   tags: []
   ---
   ```
3. 写正文

## 7. Dataview 查询示例

在任意 .md 里写：

````
```dataview
TABLE stack, difficulty, status
FROM ""
WHERE stack = "linux"
SORT updated DESC
```
````

会渲染成表格。

更多示例见 `_dashboard/progress.md`。

## 8. 维护脚本（可选）

### 批量补 frontmatter

```bash
# Windows PowerShell
cd book
python learning/notes/_scripts/migrate_notes.py --dry-run  # 先看效果
python learning/notes/_scripts/migrate_notes.py             # 实际运行
```

### 双向链接

```bash
cd book
bash learning/notes/_scripts/linkify.sh --dry-run
bash learning/notes/_scripts/linkify.sh
```

## 9. 与其他笔记系统对比

| 维度 | Obsidian Vault (本仓库) | Notion | 语雀 |
|---|---|---|---|
| 本地优先 | ✅ | ❌ | ❌ |
| Git 版本控制 | ✅ | ❌ | ❌ |
| 双链 | ✅ 原生 | 间接 | 间接 |
| Dataview 查询 | ✅ | 数据库 | 数据库 |
| 离线 | ✅ | ❌ | ❌ |
| 自定义域名 | ❌ | ✅ | ✅ |
| 协作 | ❌ | ✅ | ✅ |

## 10. 故障排查

| 问题 | 解决 |
|---|---|
| Dataview 表格不渲染 | 检查 community-plugins.json 是否含 dataview |
| Templater 模板不出现 | 检查 _templates/ 目录在 vault 内 |
| wikilink 404 | 用 `[[filename]]` 而非 `[[full/path]]` |
| Graph 视图空白 | 启用 obsidian-graph-analysis 插件 |

## 11. 进阶

- **Dataview 进阶**：<https://blacksmithgu.github.io/obsidian-dataview/>
- **Templater 文档**：<https://silentvoid13.github.io/Templater/>
- **Obsidian 官方文档**：<https://help.obsidian.md/>

---

如有问题，在仓库提 issue 即可。