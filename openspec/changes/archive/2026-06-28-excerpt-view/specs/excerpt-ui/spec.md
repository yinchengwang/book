# 摘抄 UI 规范

## ADDED Requirements

### Requirement: 左右双栏布局

系统必须以左右双栏形式展示摘抄内容，左侧为书籍导航，右侧为摘抄卡片流。

#### Scenario: 默认展示双栏布局
- **WHEN** 用户进入摘抄 Tab
- **THEN** 系统渲染左右双栏：左侧 ~280px 书籍导航栏，右侧自适应摘抄内容区

#### Scenario: 移动端切换为单栏
- **WHEN** 屏幕宽度 ≤ 768px
- **THEN** 系统隐藏左侧书籍导航栏，改为顶部下拉选择器

#### Scenario: 点击左侧书籍切换内容
- **WHEN** 用户点击左侧书籍列表中的某本书
- **THEN** 系统右侧显示该书的所有摘抄，URL hash 更新为 `#excerpt?book={id}`

### Requirement: 筛选栏

系统必须在双栏上方提供多维度筛选栏。

#### Scenario: 书籍筛选
- **WHEN** 用户点击"📚 全部书"下拉框选择某本书
- **THEN** 系统仅显示该书的摘抄

#### Scenario: 标签筛选
- **WHEN** 用户点击"🏷️ 全部标签"下拉框选择标签
- **THEN** 系统仅显示包含该标签的摘抄

#### Scenario: 年份筛选
- **WHEN** 用户点击"📅 年份"下拉框选择年份
- **THEN** 系统仅显示该年份的摘抄

#### Scenario: 批注筛选
- **WHEN** 用户点击"💬 有批注"下拉框选择"有批注"
- **THEN** 系统仅显示有 Obsidian 批注或 HTML 批注的摘抄

#### Scenario: 全文搜索
- **WHEN** 用户在搜索框中输入关键词
- **THEN** 系统实时调用 `/api/excerpts?q=关键词` 并展示匹配结果

#### Scenario: 组合筛选
- **WHEN** 用户同时选择了书 + 标签 + 年份 + 批注
- **THEN** 系统对所有筛选条件执行 AND 逻辑

#### Scenario: 筛选状态栏
- **WHEN** 用户有活跃筛选条件
- **THEN** 系统显示已选条件标签（可逐个移除），并显示匹配结果计数

### Requirement: 摘抄卡片

系统必须以卡片形式展示每条摘抄。

#### Scenario: 卡片默认展示
- **WHEN** 右侧内容区渲染摘抄卡片
- **THEN** 每张卡片显示：摘抄正文（Markdown 渲染）、书名（可点击）、作者、标签（可点击跳转）、日期、批注数、收藏状态

#### Scenario: 卡片收藏标记
- **WHEN** 摘抄被收藏（favorite: true）
- **THEN** 卡片左侧边框显示金色（#f0ad4e）

#### Scenario: 卡片批注标记
- **WHEN** 摘抄有批注（Obsidian 或 HTML）
- **THEN** 卡片元数据栏显示"💬 N 条批注"

#### Scenario: 卡片操作按钮
- **WHEN** 用户 hover 或长按卡片
- **THEN** 系统显示操作栏：📋 复制、✏️ 编辑、💬 批注、⭐ 收藏、🔗 Obsidian

### Requirement: 批注展开面板

系统必须支持在卡片内展开/折叠批注。

#### Scenario: 点击批注按钮展开
- **WHEN** 用户点击卡片上的 💬 批注按钮
- **THEN** 系统在卡片下方展开批注面板，显示所有 Obsidian 批注和 HTML 批注

#### Scenario: Obsidian 批注渲染
- **WHEN** 批注面板显示 Obsidian 来源的批注
- **THEN** 系统以引用块格式（>）渲染批注内容，标注来源为"Obsidian 批注"

#### Scenario: HTML 批注渲染
- **WHEN** 批注面板显示 HTML 来源的批注
- **THEN** 系统以普通段落格式渲染批注内容，标注来源为"HTML 批注"

#### Scenario: 添加新批注
- **WHEN** 用户在批注面板的 textarea 中输入内容
- **THEN** 系统提供"存到 HTML"和"存到 Obsidian"两个按钮

#### Scenario: 批注折叠
- **WHEN** 用户再次点击批注按钮或面板外的区域
- **THEN** 系统折叠批注面板

### Requirement: 编辑元数据弹窗

系统必须提供弹窗编辑摘抄的元数据。

#### Scenario: 打开编辑弹窗
- **WHEN** 用户点击卡片上的 ✏️ 编辑按钮
- **THEN** 系统弹出模态框，显示书名、作者、日期、章节、页码、标签等字段

#### Scenario: 编辑书名/作者/日期
- **WHEN** 用户在弹窗中修改书名、作者或日期
- **THEN** 系统实时更新表单字段

#### Scenario: 标签管理
- **WHEN** 用户在弹窗中操作标签
- **THEN** 系统支持添加新标签（输入回车）、移除已有标签（点击 ✕）、输入时自动补全已有标签

#### Scenario: 收藏切换
- **WHEN** 用户在弹窗中点击"收藏"/"取消收藏"
- **THEN** 系统更新 favorite 字段，卡片即时反映状态变化

#### Scenario: 保存策略
- **WHEN** 用户点击"保存到 Obsidian"
- **THEN** 系统调用 PUT /api/excerpts/:id/meta，saveTarget="obsidian"，写回 .md 文件 frontmatter

- **WHEN** 用户点击"存到 HTML"
- **THEN** 系统更新 localStorage 中的 user_meta

- **WHEN** 用户点击"同时保存"
- **THEN** 系统先后调用两次保存

### Requirement: 分页

系统必须在摘抄内容区底部显示分页控件。

#### Scenario: 默认分页
- **WHEN** 摘抄总数超过每页条数
- **THEN** 系统显示页码导航，默认每页 20 条

#### Scenario: 切换每页条数
- **WHEN** 用户点击"每页: [20 ▼]"
- **THEN** 系统提供 10/20/50 选项

#### Scenario: 页码导航
- **WHEN** 总页数 > 9
- **THEN** 系统显示省略号（如 [1] [2] [3] ... [25] [下一页 ▶]）

#### Scenario: 翻页保持筛选
- **WHEN** 用户翻页
- **THEN** 系统保持当前所有筛选条件（书/标签/年份/批注/搜索关键词）

### Requirement: 标签云

系统必须在左侧书籍导航底部显示热门标签云。

#### Scenario: 标签云展示
- **WHEN** 系统渲染标签云
- **THEN** 每个标签显示为可点击链接，格式为 `#标签名(数量)`

#### Scenario: 点击标签
- **WHEN** 用户点击标签云中的标签
- **THEN** 系统筛选显示该标签的所有摘抄，URL hash 更新

### Requirement: 响应式布局

系统必须适配桌面端、平板和手机端三种视口。

#### Scenario: 桌面端布局（>768px）
- **WHEN** 屏幕宽度大于 768px
- **THEN** 左右双栏布局，左侧 ~280px，右侧自适应

#### Scenario: 平板布局（481-768px）
- **WHEN** 屏幕宽度在 481-768px 之间
- **THEN** 左侧栏缩窄至 200px，卡片间距缩小

#### Scenario: 手机端布局（≤480px）
- **WHEN** 屏幕宽度小于等于 480px
- **THEN** 隐藏左侧栏，筛选栏改为顶部横向滚动条，卡片全宽显示，编辑弹窗全屏覆盖

### Requirement: 与读书雷达联动

系统必须支持从读书雷达跳转到摘抄视图。

#### Scenario: 看板卡片显示摘抄数
- **WHEN** 读书雷达看板渲染书籍卡片
- **THEN** 每张卡片底部显示"📝 N 条摘抄"（N = excerptCount）

#### Scenario: 点击摘抄数跳转
- **WHEN** 用户点击看板卡片上的"📝 N 条摘抄"
- **THEN** 系统自动切换到摘抄 Tab，并筛选该书

#### Scenario: 书籍详情页显示摘抄入口
- **WHEN** 用户打开书籍详情弹窗
- **THEN** 弹窗底部显示"查看该书所有摘抄"按钮，点击后跳转到摘抄 Tab
