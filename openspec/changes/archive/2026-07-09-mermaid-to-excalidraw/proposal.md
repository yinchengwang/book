# Proposal: Mermaid to Excalidraw 转换工具

## Summary

设计并实现一个工具，将 Mermaid 图表转换为 Excalidraw 手绘风格图表，保持语义等价的同时呈现自然的手绘效果。

## Why

### 问题背景

1. **当前图表生成方式的局限**
   - Excalidraw JSON 格式复杂，手写难度高
   - 自动生成的 Excalidraw 文件缺乏手绘风格
   - 需要在代码中定义大量坐标位置

2. **Mermaid 的优势**
   - 声明式语法，简洁直观
   - 易于版本控制和维护
   - 社区广泛使用，生态丰富

3. **用户需求**
   - 希望用 Mermaid 快速定义图表结构
   - 自动转换为 Excalidraw 手绘风格
   - 保留布局信息，避免手动调整

### 影响

- **开发者体验**: 降低图表创建门槛，提高效率
- **文档质量**: 统一的手绘风格提升可读性
- **协作效率**: Mermaid 易于 code review

## What Changes

### Capabilities

#### 1. 核心转换引擎

- **Mermaid 语法解析器**: SHALL 支持 flowcharts、sequence diagrams、class diagrams、state diagrams
  - Scenario: 输入 `flowchart TD\n A --> B` 输出包含节点 A、B 的 IR
- **布局算法**: SHALL 自动计算节点位置，避免重叠
  - Scenario: 4 节点流程图应产生正确的层次布局
- **Excalidraw 生成器**: SHALL 生成符合 Excalidraw v2 规范的 JSON
  - Scenario: 生成的 JSON 可直接导入 Excalidraw 打开

#### 2. 形状映射

- **节点形状映射**: Mermaid 节点类型 SHALL 映射到正确的 Excalidraw 形状
  - `rect` → rectangle
  - `diamond` → diamond
  - `circle` → ellipse
  - Scenario: `A[Label]` 应渲染为矩形框

#### 3. 边处理

- **箭头生成**: SHALL 生成带箭头的连接线
  - Scenario: `A --> B` 应产生从 A 指向 B 的箭头
- **标签支持**: SHALL 支持边上的文字标签
  - Scenario: `A -->|yes| B` 应在边上显示 "yes" 标签

#### 4. 样式应用

- **手绘风格**: SHALL 应用手绘风格参数（线条粗细 2px、颜色 #1a1a1a）
- **颜色主题**: SHALL 支持 light 和 dark 主题
  - Scenario: `--theme dark` 应生成深色主题的图表

#### 5. 命令行工具

- **convert 命令**: SHALL 支持单文件转换
  - Scenario: `mermaid2excalidraw convert input.mmd -o output.json` 应成功转换
- **batch 命令**: SHALL 支持批量转换目录
  - Scenario: 批量转换 10 个文件应全部成功
- **preview 命令**: SHALL 支持预览输出到 stdout

## Impact

### 新增文件

- `tools/mermaid2excalidraw/` - 工具主目录
- `tools/mermaid2excalidraw/main.py` - 入口文件
- `tools/mermaid2excalidraw/parser/` - Mermaid 解析器
- `tools/mermaid2excalidraw/layout/` - 布局算法
- `tools/mermaid2excalidraw/generator/` - Excalidraw 生成器
- `tools/mermaid2excalidraw/tests/` - 测试文件

### 风险与缓解

| Risk | Impact | Mitigation |
|------|--------|------------|
| 复杂布局转换不准确 | 中 | 提供手动微调参数 |
| 部分 Mermaid 语法不支持 | 低 | 优先支持常用语法，逐步扩展 |
| Excalidraw 格式兼容性问题 | 中 | 使用稳定版本的 schema |
