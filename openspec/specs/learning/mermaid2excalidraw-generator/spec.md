# Spec: Mermaid2Excalidraw Generator

## Purpose

将 Mermaid 图表的中间表示转换为 Excalidraw JSON 格式。

## Requirements

### Requirement: Excalidraw JSON 生成

生成器 SHALL 生成符合 Excalidraw v2 规范的 JSON：

##### Scenario: 基本 JSON 输出

- **WHEN** 生成一个简单流程图的 Excalidraw JSON
- **THEN** 输出 SHALL 包含 type、version、elements、appState 字段

### Requirement: 形状映射

| Mermaid 形状 | Excalidraw 类型 | 参数 |
|--------------|-----------------|------|
| `rect` / `[text]` | rectangle | width, height |
| `stadium` / `(text)` | rectangle (rounded) | width, height, borderRadius |
| `subroutine` / `[[text]]` | rectangle (double) | - |
| `cylinder` / `[(text)]` | ellipse (压扁) | - |
| `diamond` / `{text}` | diamond 或 text | - |
| `circle` / `((text))` | ellipse | - |
| `hexagon` | rectangle (六边形路径) | - |
| `parallelogram` / `/text/` | parallelogram | - |

##### Scenario: 矩形节点生成

- **WHEN** 生成 `A[Start]` 节点
- **THEN** SHALL 输出 type="rectangle" 的元素

##### Scenario: 菱形节点生成

- **WHEN** 生成 `B{Decision}` 节点
- **THEN** SHALL 输出 type="text" 或 type="diamond" 的元素

### Requirement: 手绘风格参数

所有元素 SHALL 应用以下手绘风格：

##### Scenario: 样式参数验证

- **WHEN** 生成任何元素
- **THEN** SHALL 包含 strokeColor="#1a1a1a"、strokeWidth=2、fontSize=14

### Requirement: 颜色主题

生成器 SHALL 支持两种颜色主题：

**Light 主题**:
- 矩形: #e7f5ff (浅蓝)
- 菱形: #fff9db (浅黄)
- 椭圆: #e7ffe7 (浅绿)
- 文字: #1a1a1a

**Dark 主题**:
- 矩形: #2d3748
- 菱形: #744210
- 椭圆: #22543d
- 文字: #f7fafc

##### Scenario: 主题切换

- **WHEN** 使用 `--theme dark` 参数
- **THEN** SHALL 生成深色主题的颜色值

### Requirement: 箭头生成

边 SHALL 生成为 Excalidraw Arrow 元素：

##### Scenario: 箭头元素生成

- **WHEN** 生成边 `A --> B`
- **THEN** SHALL 输出 type="arrow" 的元素

### Requirement: 标签位置

带标签的边 SHALL 在边的中点上方添加文本元素：

##### Scenario: 边标签

- **WHEN** 生成带标签的边 `A -->|yes| B`
- **THEN** SHALL 输出标签文本元素在边中点附近
