# 摘抄数据层规范

## ADDED Requirements

### Requirement: Markdown 文件存储结构

系统必须以 Markdown 文件 + YAML frontmatter 的方式持久化摘抄数据。

#### Scenario: 摘抄文件结构
- **WHEN** 系统读取摘抄文件
- **THEN** 每个摘抄文件以 `---` 分隔的 YAML frontmatter 开头，包含 book/author/date/tags/favorite/chapter/page 字段，正文为摘抄内容

#### Scenario: frontmatter 解析
- **WHEN** 系统解析 .md 文件
- **THEN** 系统提取 frontmatter 中的结构化数据，正文部分按 `---` 分隔为多条摘抄

#### Scenario: 缺失 frontmatter 的回退
- **WHEN** 读取的 .md 文件没有 YAML frontmatter
- **THEN** 系统从文件名推断 book 字段，date 从目录推断，tags 为空数组

### Requirement: 批注存储格式

系统必须以 Obsidian 引用块格式存储 Obsidian 批注。

#### Scenario: Obsidian 批注格式
- **WHEN** 用户在 Obsidian 中为摘抄添加批注
- **THEN** 批注以 `> **批注**: 内容` 的引用块格式追加在摘抄正文后

#### Scenario: HTML 批注存储
- **WHEN** 用户在 HTML 界面添加批注并选择"存到 HTML"
- **THEN** 系统将该批注存入 localStorage key `excerpt-notes-{bookId}-{excerptIndex}`

#### Scenario: 批注合并展示
- **WHEN** 系统渲染摘抄卡片的批注面板
- **THEN** 系统优先展示 Obsidian 批注（从 .md 文件读取），其次展示 HTML 批注（从 localStorage 读取）

### Requirement: Obsidian 双向链接

系统必须支持与 Obsidian 的双向链接。

#### Scenario: Obsidian URI Scheme
- **WHEN** 用户点击卡片上的 🔗 Obsidian 按钮
- **THEN** 系统打开 `obsidian://open?file={filePath}&heading={headingId}` URI，在 Obsidian 中定位到对应摘抄

#### Scenario: Wikilink 解析
- **WHEN** 批注内容中包含 `[[笔记名]]` 格式
- **THEN** 系统将其渲染为可点击链接，点击后在 Obsidian 中打开对应笔记

#### Scenario: 书签格式
- **WHEN** 用户点击书签按钮
- **THEN** 系统复制当前摘抄的 Obsidian URI 到剪贴板，格式为 `obsidian://open?file=...`

### Requirement: 数据迁移

系统必须支持将现有 .txt 文件批量转换为 .md 文件。

#### Scenario: .txt 文件扫描
- **WHEN** 运行迁移脚本
- **THEN** 系统扫描 `notes/excerpt/` 下所有 .txt 文件

#### Scenario: 书名推断
- **WHEN** 转换 .txt 文件
- **THEN** 系统从文件名（去掉 `_xxx.txt` 后缀）或目录名推断 book 字段

#### Scenario: 段落拆分
- **WHEN** 转换 .txt 文件
- **THEN** 系统按空行将内容拆分为多条摘抄，每条生成独立 excerpt

#### Scenario: 标签推断
- **WHEN** 转换 .txt 文件
- **THEN** 系统从文件名中的关键词（如"专业""责任"）或目录名推断 tags

#### Scenario: 年份推断
- **WHEN** 转换 .txt 文件
- **THEN** 系统从所在目录（如 `2025/` 或 `2024/`）推断 date 字段的年份

#### Scenario: 备份原始文件
- **WHEN** 转换完成
- **THEN** 系统为每个原始 .txt 文件创建 `.txt.bak` 备份

#### Scenario: 编码转换
- **WHEN** 读取 .txt 文件
- **THEN** 系统检测文件编码（GBK/UTF-8），统一转为 UTF-8 写入 .md

### Requirement: localStorage 降级存储

当 server.js 不可用时，系统必须降级到 localStorage 存储用户元数据。

#### Scenario: 离线环境自动降级
- **WHEN** 页面在 file:// 协议下打开且无法连接 server.js
- **THEN** 系统使用 localStorage key `excerpt-meta-{bookId}` 存储用户的标签修改/收藏/批注

#### Scenario: 离线编辑缓存
- **WHEN** 用户在离线环境下编辑摘抄元数据
- **THEN** 系统立即写入 localStorage 缓存

#### Scenario: 在线时自动同步
- **WHEN** 用户从 file:// 切换到 http:// 且有 server.js
- **THEN** 系统检测变化，将 localStorage 中的元数据写入服务器
