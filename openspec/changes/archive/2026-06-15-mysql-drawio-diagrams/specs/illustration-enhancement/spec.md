## ADDED Requirements

### Requirement: MD 文章嵌入 drawio 图片
系统 SHALL 在 MD 学习文章中嵌入 drawio 生成的 PNG 图片，遵循以下规范：

1. 图片引用使用相对路径：`![图标题](../../../illustrations/<product>/<topic>/<image>.png)`
2. 图片容器使用 `<div class="diagram-block">` 包裹，支持居中显示和说明文字
3. 每张图下方必须有中文说明文字（1-2 句）
4. 复杂架构图必须使用编号标注（①Buffer Pool ②Free List 等）

#### Scenario: 在 MD 文章中嵌入 Buffer Pool 图
- **WHEN** 渲染 `db-buffer-pool.md` 文章
- **THEN** 文章中包含 `<div class="diagram-block"><img src=".../buffer_pool_diagram.png" alt="Buffer Pool 内存分布"></div>` 且下方有说明文字

#### Scenario: 在 MD 文章中嵌入锁类型图
- **WHEN** 渲染 `db-locking.md` 文章
- **THEN** 文章中包含 S锁/X锁、Record锁、Gap锁 的对比示意图

### Requirement: 渲染增强
系统 SHALL 支持 drawio 图片在 learn.html 中的正确渲染：

1. 图片容器 `.diagram-block` 已有 CSS 样式（learn.html L179）
2. 图片必须居中显示，最大宽度 900px
3. 图片下方说明文字使用 `<p class="diagram-caption">` 样式
4. 移动端适配：图片宽度自适应屏幕

#### Scenario: 学习页渲染 drawio 图片
- **WHEN** 用户打开 `learn.html#vdb/db-buffer-pool`
- **THEN** Buffer Pool 架构图居中显示，下方有中文说明

#### Scenario: 移动端适配
- **WHEN** 用户在移动设备上打开学习页
- **THEN** drawio 图片宽度自适应屏幕，不溢出
