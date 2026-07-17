# 面试追踪 — 设计文档

## Context

reading-radar 是一个纯静态 HTML/JS 学习站点，无构建工具。所有页面通过 `node server.js` 本地服务访问，数据存储在 `user-data/` 目录下由 Git 追踪。

现有 `interview.html` 是面试题库页面（只读浏览），需要在导航栏旁新增"面试追踪"页面，作为个人面试管线管理工具。

**关键约束**：
- 零外部依赖（复用现有 marked.js CDN）
- 所有用户数据以 Markdown 文件存储在磁盘上，Git 可追踪
- 支持有 server.js（http://）和无 server.js（file://）两种模式
- 风格必须与现有页面（index.html, interview.html）统一

## Goals / Non-Goals

**Goals:**
- 完整的面试管线追踪：公司卡片、状态流转、多轮面试笔记、JD 记录
- Markdown 文件存储，支持 Obsidian 直接编辑
- server.js API + localStorage 双模式存储
- 时间线分组 + 多维过滤（月份/地域/搜索/排序）
- 响应式布局（桌面/平板/手机）
- 与现有阅读雷达风格完全统一

**Non-Goals:**
- 云端同步（无需 OAuth/注册，数据仅在本地）
- 团队协作（纯个人使用）
- 日历视图（仅时间线按月分组）
- 自动化简历匹配（手动记录）
- 邮件/消息集成

## Decisions

### Decision 1: Markdown + JSON 混合存储

**选择**：公司元数据用 `_index.json`（快速查询），正文内容用 Markdown 文件。

**理由**：
- JSON 索引支持 O(1) 的公司列表查询和过滤（不需要解析所有 MD 文件）
- Markdown 正文可用 Obsidian 编辑、版本追踪、支持代码块/表格/图片
- 与现有 `data/learn-deep/` 目录模式一致

**替代方案**：纯 JSON 存储
- 舍弃原因：面试笔记包含代码块和格式化文本，Markdown 更合适

### Decision 2: 详情面板用底部 Sheet（非侧边栏）

**选择**：点击卡片后从底部弹出 Sheet 覆盖下半屏。

**理由**：
- 与现有 `index.html` 的书籍详情弹窗风格一致
- 移动端体验更好（不遮挡卡片列表）
- 桌面端可在卡片上方展示，不覆盖其他卡片

**替代方案**：侧边栏滑出
- 舍弃原因：需要重新布局卡片网格，复杂度高

### Decision 3: 数据同步用 server.js API（非 IndexedDB）

**选择**：通过 `server.js` 的 `/api/tracker/*` 读写 Markdown 文件。

**理由**：
- 现有 reading-radar 所有数据持久化都走 server.js API
- 服务器端保存，关闭浏览器不丢失
- Git 追踪，换设备只需同步仓库

**降级策略**：file:// 协议时自动降级到 localStorage

### Decision 4: 公司文件夹命名用 `{company}-{position}` 格式

**选择**：`data/interview-tracker/byte-dccp/`（公司拼音-岗位拼音）。

**理由**：
- 目录名可读、可搜索
- 避免中文路径在 Windows 上的兼容性问题

**替代方案**：UUID
- 舍弃原因：目录名不可读，调试困难

### Decision 5: 面试笔记用 Markdown（复用 marked.js）

**选择**：所有笔记字段（JD、技术问题、综合面、备注）都用 Markdown 格式。

**理由**：
- `interview.html` 已加载 marked.js + highlight.js，无需新增 CDN
- 支持代码高亮（技术问题常含代码示例）
- 与面试题库页面格式统一

### Decision 6: 状态变更触发时间线移动，内容变更不触发

**选择**：只有状态变更（或手动"更新月份"操作）才会让卡片跨月移动。仅修改 JD/笔记不触发移动。

**理由**：
- 避免用户编辑笔记时卡片跳动，体验干扰
- 时间线的语义是"本月面试进展"，而非"本月编辑过"

## Risks / Trade-offs

### Risk 1: Markdown 编辑冲突
[两个标签页同时编辑同一公司] → 后保存的覆盖先保存的
**Mitigation**: MVP 阶段不做并发锁，在 UI 上加提示"正在编辑中..."

### Risk 2: file:// 模式 API 不可用
[用户直接打开 HTML 文件而非通过 server.js] → API 调用失败
**Mitigation**: 自动检测 `window.location.protocol`，降级到 localStorage，并提供"启动本地服务器"提示

### Risk 3: 公司文件夹数量增长
[用户积累 100+ 公司后，文件数量增多] → 索引加载变慢
**Mitigation**: `_index.json` 保持轻量（仅元数据），全文内容按需加载

### Risk 4: Markdown 编辑学习成本
[用户不熟悉 Markdown 语法] → 编辑体验差
**Mitigation**: 编辑时提供简易工具栏（加粗/标题/代码块/列表），默认生成格式化的模板

## Migration Plan

**零迁移成本**：这是全新功能，不影响任何现有页面或数据。

**部署步骤**：
1. 创建 `interview-tracker.html`
2. 创建 `data/app/interview-tracker.js`
3. 修改 `nav-component.js` 添加导航项
4. 修改 `server.js` 添加 API 路由
5. 修改 `interview.html` 标题
6. 创建 `data/interview-tracker/` 空目录 + `_index.json`
7. 本地验证：`node server.js` → 浏览器打开 → 检查面试追踪页面

**回滚**：删除新增文件，恢复被修改的文件即可。

## Open Questions

1. **公司名字典**：MVP 阶段选择自由输入（方案 B），是否需要内置 200+ 公司名的自动补全作为后续增强？
2. **ZIP 导出/导入**：MVP 阶段是否包含 ZIP 压缩打包？还是先用 JSON 导出/导入？（JSON 足够 MVP 使用）
3. **图片附件**：面试笔记中是否需要支持上传截图（如面经截图）？MVP 暂不包含，后续可通过 `/api/tracker/{id}/attachments/` 扩展
