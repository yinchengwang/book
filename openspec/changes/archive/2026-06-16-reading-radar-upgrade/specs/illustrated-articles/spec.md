# 图解穿插展示

## Purpose

知识点文章内容支持内嵌图解图片，渲染后图片居中显示并附带说明文字，穿插在章节之间辅助理解。

## ADDED Requirements

### Requirement: Markdown 图片渲染

`learn.html` SHALL 将 Markdown 中的 `![说明文字](路径)` 语法渲染为 `<img>` 元素，图片 SHALL 居中显示，最大宽度不超过内容区宽度的 90%，并附带说明文字（caption）。

#### Scenario: 渲染图片居中

- **WHEN** 知识点 MD 文件中包含 `![内存就像一排信箱](./diagrams/pointer-memory-map.png)`
- **THEN** 页面 SHALL 渲染为居中的 `<img>` 元素，带说明文字 `<figcaption>内存就像一排信箱</figcaption>`

#### Scenario: 图片响应式缩放

- **WHEN** 用户在移动端或小窗口查看含图片的知识点
- **THEN** 图片 SHALL 自动缩放至不超过内容区宽度的 90%，保持宽高比

#### Scenario: 图片加载失败

- **WHEN** 图解图片文件不存在
- **THEN** 页面 SHALL 显示占位符提示"图解图片缺失"，不阻断内容阅读

### Requirement: 图解文件路径约定

每个知识点的图解图片 SHALL 存放在同级目录下的 `diagrams/` 子目录中。例如 `c-pointer.md` 的图解存放在 `data/learn-deep/c/language/diagrams/` 目录下。Markdown 中使用相对路径引用：`![说明](./diagrams/图片名.后缀)`。

#### Scenario: 图解文件路径解析

- **WHEN** 知识点 `c-pointer.md` 包含 `![图1](./diagrams/pointer-memory-map.png)`
- **THEN** 页面 SHALL 从 `data/learn-deep/c/language/diagrams/pointer-memory-map.png` 加载图片

#### Scenario: 图解文件不存在时的降级

- **WHEN** 引用的图解图片文件不存在
- **THEN** 页面 SHALL 显示占位符，不报 404 错误
