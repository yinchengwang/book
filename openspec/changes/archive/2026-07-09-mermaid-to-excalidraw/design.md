# Design: Mermaid to Excalidraw 转换工具

## Context

### 背景

用户希望用简洁的 Mermaid 语法定义图表，然后自动转换为具有手绘风格的 Excalidraw JSON 文件。

### 约束

- 输出必须符合 Excalidraw v2 JSON 格式
- 保持 Mermaid 语义等价性
- 自动布局避免手动调整
- 支持命令行和可能的 VSCode 集成

### 当前状态

- 无现有实现
- 需要从头设计

## Goals / Non-Goals

**Goals:**
- 支持 flowchart、sequence diagram、class diagram、state diagram
- 自动布局算法生成合理位置
- 手绘风格参数可配置
- 简单易用的命令行接口

**Non-Goals:**
- 不支持 Mermaid 的所有语法（先支持常用子集）
- 不提供 GUI 编辑器（只做转换）
- 不实现双向转换（Excalidraw → Mermaid）

## Decisions

### Decision 1: 编程语言选择

| 选项 | 优点 | 缺点 |
|------|------|------|
| **Go** | 性能好，单二进制部署，类型安全 | 生态较小 |
| **Python** | 生态丰富，Mermaid parser 成熟 | 需运行环境，性能一般 |
| **TypeScript** | 与 Excalidraw 同生态，JSON 处理自然 | 需 Node.js |

**选择**: Python
- 原因: `mermaid-js` 有成熟的 Python 绑定 `mermaid-cli` 可通过子进程调用
- 或直接用正则解析 Mermaid 语法（更轻量）

### Decision 2: 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                     命令行入口 (CLI)                         │
│         argparse / click → 命令分发                         │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────┐
│                    解析层 (Parser)                           │
│         Mermaid 文本 → 中间表示 (IR)                        │
│                                                              │
│  支持语法:                                                   │
│  - flowchart: node, edge, subgraph                          │
│  - sequence: participant, arrow, note                       │
│  - class: class, method, relation                           │
│  - state: state, transition                                 │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────┐
│                    布局引擎 (Layout)                         │
│         IR → 带坐标的节点列表                               │
│                                                              │
│  算法: Dagre (有向图布局) 或自定义层次布局                   │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────┐
│                    样式映射 (Style)                          │
│         节点类型 → Excalidraw 形状 + 颜色                   │
│                                                              │
│  rect → rectangle (fill: #e7f5ff)                           │
│  diamond → ellipse (fill: #fff9db)                          │
│  circle → ellipse (fill: #2f9e44)                           │
│  ...                                                         │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────┐
│                   生成器 (Generator)                         │
│         样式化 IR → Excalidraw JSON                        │
│                                                              │
│  输出符合 excalidraw v2 schema                              │
└─────────────────────────────────────────────────────────────┘
```

### Decision 3: Excalidraw JSON 结构

```json
{
  "type": "excalidraw",
  "version": 2,
  "source": "mermaid2excalidraw",
  "elements": [
    {
      "id": "<unique>",
      "type": "rectangle|ellipse|arrow|text|...",
      "x": 100,
      "y": 200,
      "width": 120,
      "height": 60,
      "fillStyle": "solid|hachure|cross-hatch",
      "fillColor": "#e7f5ff",
      "strokeColor": "#1a1a1a",
      "strokeWidth": 2,
      "fontSize": 14,
      "fontFamily": 1
    }
  ],
  "appState": {
    "gridSize": null,
    "viewBackgroundColor": "#ffffff"
  }
}
```

### Decision 4: 布局算法

采用 **层次布局 (Layered Layout)**:

1. **拓扑排序**: 确定节点层级
2. **层次分配**: BFS/DFS 分配层号
3. **同层排序**: 减少交叉
4. **坐标计算**: 每层居中，节点均匀分布

参数:
- `nodeWidth`: 120px
- `nodeHeight`: 60px
- `hSpacing`: 50px (水平间距)
- `vSpacing`: 80px (垂直间距)

### Decision 5: 文件结构

```
tools/mermaid2excalidraw/
├── main.py              # CLI 入口
├── parser/
│   ├── __init__.py
│   ├── flowchart.py     # 流程图解析
│   ├── sequence.py      # 时序图解析
│   └── ir.py            # 中间表示定义
├── layout/
│   ├── __init__.py
│   └── layered.py       # 层次布局算法
├── style/
│   ├── __init__.py
│   └── default.py       # 默认样式
├── generator/
│   ├── __init__.py
│   └── excalidraw.py    # Excalidraw JSON 生成
└── config.py            # 配置管理
```

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| 复杂图形布局不美观 | 中 | 提供手动位置覆盖参数 |
| Mermaid 子图展开 | 低 | 扁平化处理或递归布局 |
| 箭头标签位置 | 低 | 智能居中放置 |

## Open Questions

1. 是否支持 Mermaid 的实时预览？
2. 是否需要保存原始 Mermaid 注释到 Excalidraw？
3. 是否支持批量转换配置？
