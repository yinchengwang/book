# 摘抄 API 规范

## ADDED Requirements

### Requirement: 书籍列表 API

系统必须提供 API 返回所有摘抄书籍的元数据。

#### Scenario: 获取书籍列表
- **WHEN** 客户端发送 GET `/api/excerpts/books`
- **THEN** 系统返回 JSON，包含：
  - `books`: 数组，每项含 `{ id, title, count, tags, year, noteCount }`
  - `allTags`: 所有出现过的标签数组（含计数）
  - `years`: 所有年份数组
  - `totalExcerpts`: 摘抄总数

#### Scenario: 书籍列表缓存
- **WHEN** 连续请求 GET `/api/excerpts/books`
- **THEN** 系统返回缓存结果（TTL 60 秒），避免重复扫描文件系统

### Requirement: 摘抄列表 API（分页）

系统必须提供分页的摘抄列表查询。

#### Scenario: 获取全部摘抄（第一页）
- **WHEN** 客户端发送 GET `/api/excerpts?page=1&per_page=20`
- **THEN** 系统返回：
  - `excerpts`: 摘抄数组，每项含 `{ id, book, bookId, body, author, date, chapter, tags, notes[], favorite }`
  - `pagination`: `{ page, per_page, total, pages }`

#### Scenario: 按书筛选
- **WHEN** 客户端发送 GET `/api/excerpts?book=clean-code&page=1`
- **THEN** 系统仅返回该书的所有摘抄

#### Scenario: 按标签筛选
- **WHEN** 客户端发送 GET `/api/excerpts?tag=专业主义&page=1`
- **THEN** 系统仅返回包含该标签的所有摘抄

#### Scenario: 按年份筛选
- **WHEN** 客户端发送 GET `/api/excerpts?year=2025&page=1`
- **THEN** 系统仅返回 2025 年的摘抄

#### Scenario: 按批注筛选
- **WHEN** 客户端发送 GET `/api/excerpts?has_note=true&page=1`
- **THEN** 系统仅返回有 Obsidian 批注或 HTML 批注的摘抄

#### Scenario: 按收藏筛选
- **WHEN** 客户端发送 GET `/api/excerpts?favorite=true&page=1`
- **THEN** 系统仅返回收藏的摘抄

#### Scenario: 全文搜索
- **WHEN** 客户端发送 GET `/api/excerpts?q=专业&page=1`
- **THEN** 系统在摘抄正文 + 书名 + 标签中搜索关键词，返回匹配结果

#### Scenario: 组合筛选
- **WHEN** 客户端发送 GET `/api/excerpts?book=clean-code&tag=专业主义&q=责任&page=1`
- **THEN** 系统对所有条件执行 AND 逻辑

#### Scenario: 无效页码
- **WHEN** 客户端发送 GET `/api/excerpts?page=999`
- **THEN** 系统返回空数组 `excerpts: [], pagination: { page: 999, total: 0, pages: 0 }`

#### Scenario: 无效 per_page
- **WHEN** 客户端发送 GET `/api/excerpts?per_page=1000`
- **THEN** 系统限制为最大值 100

### Requirement: 元数据更新 API

系统必须提供 API 更新摘抄的元数据。

#### Scenario: 更新元数据（保存 Obsidian）
- **WHEN** 客户端发送 PUT `/api/excerpts/:id/meta`，body 为 `{ book, author, date, tags, favorite, chapter, page, saveTarget: "obsidian" }`
- **THEN** 系统更新对应 .md 文件的 frontmatter，返回 `{ success: true, updated: "notes/excerpt/.../file.md" }`

#### Scenario: 更新元数据（保存 HTML）
- **WHEN** 客户端发送 PUT `/api/excerpts/:id/meta`，body 为 `{ saveTarget: "html" }`
- **THEN** 系统更新 localStorage 中的用户元数据

#### Scenario: 更新元数据（同时保存）
- **WHEN** 客户端发送 PUT `/api/excerpts/:id/meta`，body 为 `{ saveTarget: "both" }`
- **THEN** 系统先后写回 .md 文件和 localStorage

#### Scenario: 不存在的摘抄 ID
- **WHEN** 客户端发送 PUT `/api/excerpts/nonexistent/meta`
- **THEN** 系统返回 404 `{ error: "excerpt not found" }`

### Requirement: 批注管理 API

系统必须提供 API 管理摘抄的批注。

#### Scenario: 添加批注（HTML）
- **WHEN** 客户端发送 POST `/api/excerpts/:id/note`，body 为 `{ content: "批注内容", source: "html" }`
- **THEN** 系统存入 localStorage，返回 `{ success: true }`

#### Scenario: 添加批注（Obsidian）
- **WHEN** 客户端发送 POST `/api/excerpts/:id/note`，body 为 `{ content: "批注内容", source: "obsidian" }`
- **THEN** 系统追加到对应 .md 文件的 frontmatter 后的引用块中

#### Scenario: 获取批注列表
- **WHEN** 客户端发送 GET `/api/excerpts/:id/notes`
- **THEN** 系统返回该摘抄的所有批注（合并 Obsidian + HTML 来源）

#### Scenario: 删除批注
- **WHEN** 客户端发送 DELETE `/api/excerpts/:id/note/:noteId`
- **THEN** 系统删除指定批注，返回 `{ success: true }`

### Requirement: 收藏管理 API

系统必须提供 API 管理摘抄的收藏状态。

#### Scenario: 收藏摘抄
- **WHEN** 客户端发送 PUT `/api/excerpts/:id/favorite`，body 为 `{ favorite: true }`
- **THEN** 系统更新 frontmatter 中的 favorite 字段

#### Scenario: 取消收藏
- **WHEN** 客户端发送 PUT `/api/excerpts/:id/favorite`，body 为 `{ favorite: false }`
- **THEN** 系统更新 frontmatter 中的 favorite 字段

### Requirement: 搜索建议（Auto-complete）

系统必须提供搜索建议 API。

#### Scenario: 标签建议
- **WHEN** 客户端发送 GET `/api/excerpts/suggestions?type=tag&q=专`
- **THEN** 系统返回匹配"专"字的标签列表（如 `[{ tag: "专业主义", count: 42 }]`）

#### Scenario: 书名建议
- **WHEN** 客户端发送 GET `/api/excerpts/suggestions?type=book&q=代码`
- **THEN** 系统返回匹配"代码"的书名列表（如 `[{ id: "clean-code", title: "代码整洁之道", count: 42 }]`）

### Requirement: 导出 API

系统必须提供 API 导出摘抄数据。

#### Scenario: 导出为 JSON
- **WHEN** 客户端发送 GET `/api/excerpts/export?format=json`
- **THEN** 系统返回所有摘抄的 JSON 文件下载

#### Scenario: 导出为 Markdown
- **WHEN** 客户端发送 GET `/api/excerpts/export?format=markdown`
- **THEN** 系统返回所有摘抄拼接的 Markdown 文件下载

#### Scenario: 导出筛选后的摘抄
- **WHEN** 客户端发送 GET `/api/excerpts/export?format=zip&book=clean-code`
- **THEN** 系统仅导出该书的摘抄

### Requirement: 安全防护

系统必须防止路径遍历攻击。

#### Scenario: 路径遍历拒绝
- **WHEN** API 接收到包含 `../` 或 `..\\` 的请求参数
- **THEN** 系统拒绝请求并返回 403 错误

#### Scenario: 非法文件扩展名拒绝
- **WHEN** API 接收到非 `.md`/`.txt` 的文件路径
- **THEN** 系统拒绝请求并返回 403 错误
