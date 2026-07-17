# 面试追踪数据层规范

## ADDED Requirements

### Requirement: Markdown 文件存储结构
系统必须以 Markdown 文件 + JSON 索引的方式持久化公司数据。

#### Scenario: 公司数据文件目录
- **WHEN** 创建新公司记录
- **THEN** 系统在 `data/interview-tracker/{company-id}/` 目录下生成 `_meta.md`、`jd.md`、`comprehensive.md`、`notes.md` 文件，并在 `interviews/` 子目录下生成各轮面试文件

#### Scenario: 索引文件维护
- **WHEN** 公司记录被创建、更新或删除
- **THEN** 系统同步更新 `data/interview-tracker/_index.json` 索引文件，包含公司名、岗位、状态、创建/更新日期等元数据

### Requirement: server.js API 路由
系统必须通过 server.js 提供 `/api/tracker/*` RESTful API。

#### Scenario: 获取公司索引
- **WHEN** 客户端发送 GET `/api/tracker/index`
- **THEN** 系统返回 `_index.json` 的内容（JSON 格式的公司列表）

#### Scenario: 保存公司索引
- **WHEN** 客户端发送 PUT `/api/tracker/index` 且 body 为 JSON
- **THEN** 系统保存索引到 `_index.json` 并返回 `{ ok: true }`

#### Scenario: 获取公司信息
- **WHEN** 客户端发送 GET `/api/tracker/{companyId}/meta`
- **THEN** 系统返回 `data/interview-tracker/{companyId}/_meta.md` 的文件内容

#### Scenario: 保存公司信息
- **WHEN** 客户端发送 PUT `/api/tracker/{companyId}/meta` 且 body 为文本
- **THEN** 系统保存文本到 `_meta.md` 并返回 `{ ok: true }`

#### Scenario: 获取面试轮次记录
- **WHEN** 客户端发送 GET `/api/tracker/{companyId}/interviews/{roundId}`
- **THEN** 系统返回对应 `roundId.md` 文件内容

#### Scenario: 保存面试轮次记录
- **WHEN** 客户端发送 PUT `/api/tracker/{companyId}/interviews/{roundId}` 且 body 为文本
- **THEN** 系统保存文本到对应文件并返回 `{ ok: true }`

#### Scenario: 获取 JD 记录
- **WHEN** 客户端发送 GET `/api/tracker/{companyId}/jd`
- **THEN** 系统返回 `jd.md` 文件内容

#### Scenario: 保存 JD 记录
- **WHEN** 客户端发送 PUT `/api/tracker/{companyId}/jd` 且 body 为文本
- **THEN** 系统保存文本到 `jd.md` 并返回 `{ ok: true }`

#### Scenario: 获取综合面记录
- **WHEN** 客户端发送 GET `/api/tracker/{companyId}/comprehensive`
- **THEN** 系统返回 `comprehensive.md` 文件内容

#### Scenario: 保存综合面记录
- **WHEN** 客户端发送 PUT `/api/tracker/{companyId}/comprehensive` 且 body 为文本
- **THEN** 系统保存文本到 `comprehensive.md` 并返回 `{ ok: true }`

#### Scenario: 获取备注
- **WHEN** 客户端发送 GET `/api/tracker/{companyId}/notes`
- **THEN** 系统返回 `notes.md` 文件内容

#### Scenario: 保存备注
- **WHEN** 客户端发送 PUT `/api/tracker/{companyId}/notes` 且 body 为文本
- **THEN** 系统保存文本到 `notes.md` 并返回 `{ ok: true }`

#### Scenario: 创建公司
- **WHEN** 客户端发送 POST `/api/tracker/` 且 body 为包含公司信息的 JSON
- **THEN** 系统在 `data/interview-tracker/` 下创建对应文件夹及初始文件，更新索引，返回 `{ ok: true, id: "..." }`

#### Scenario: 删除公司
- **WHEN** 客户端发送 DELETE `/api/tracker/{companyId}`
- **THEN** 系统删除该公司所有文件及索引条目，返回 `{ ok: true }`

#### Scenario: API 安全防护
- **WHEN** API 接收到包含路径遍历字符的请求
- **THEN** 系统拒绝请求并返回 403 错误

### Requirement: localStorage 降级存储
当 server.js 不可用时（file:// 协议），系统必须降级到 localStorage 存储。

#### Scenario: 离线环境自动降级
- **WHEN** 页面在 file:// 协议下打开且无法连接 server.js
- **THEN** 系统使用 localStorage key `interview-tracker-data` 存储全部数据

#### Scenario: 离线编辑缓存
- **WHEN** 用户在离线环境下编辑公司数据
- **THEN** 系统立即写入 localStorage 缓存

#### Scenario: 离线导出
- **WHEN** 用户在离线环境下点击"导出"
- **THEN** 系统从 localStorage 读取数据并生成可下载的 JSON 文件

### Requirement: 数据导出导入
系统必须支持导出全部数据为 JSON 文件，以及从 JSON 文件导入。

#### Scenario: 导出全部数据
- **WHEN** 用户点击"导出为 JSON"按钮
- **THEN** 系统将所有公司数据（含所有文件内容）打包为 JSON 文件并触发浏览器下载

#### Scenario: 导入数据
- **WHEN** 用户选择之前导出的 JSON 文件并确认导入
- **THEN** 系统解析 JSON 数据，创建/更新对应公司记录，刷新页面显示
