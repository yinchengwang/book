# Spec: layout

## ADDED Requirements

### Requirement: 层次布局算法

布局引擎 SHALL 使用层次布局算法为 flowchart 生成坐标：

1. **层级分配**: 使用最长路径算法分配节点层级
2. **同层排序**: 按拓扑顺序或减少交叉排序
3. **坐标计算**:
   - Level 0 在顶部，后续层级依次向下
   - 同层节点水平居中，均匀分布

##### Scenario: 简单流程图布局

- **WHEN** 处理 `A --> B --> C`
- **THEN** A 在 (400, 100), B 在 (400, 220), C 在 (400, 340)

##### Scenario: 分支流程图布局

- **WHEN** 处理 `A --> B --> C --> D` 和 `A --> X --> Y --> D`
- **THEN** 节点 SHALL 按层级正确排列

### Requirement: 布局参数

布局 SHALL 支持以下可配置参数：

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `node_width` | 节点宽度 | 120 |
| `node_height` | 节点高度 | 60 |
| `h_spacing` | 水平节点间距 | 50 |
| `v_spacing` | 垂直层级间距 | 80 |
| `margin_left` | 左边距 | 50 |
| `margin_top` | 顶边距 | 50 |

##### Scenario: 自定义参数

- **WHEN** 使用自定义 `node_width=200, h_spacing=80` 调用布局
- **THEN** 节点 SHALL 按新参数计算坐标

### Requirement: 边路由

边 SHALL 绘制为带箭头的直线或折线：

- **简单边**: 从源节点底部中心到目标节点顶部中心
- **跨层边**: 优先直线，必要时使用折线避免穿过节点

##### Scenario: 边点生成

- **WHEN** 布局完成后
- **THEN** 每条边 SHALL 包含至少 2 个点 (起点和终点)

### Requirement: 子图布局

子图 SHALL 作为特殊容器处理：

- 计算子图内所有节点的边界框
- 添加子图边框 (rectangle with dashed stroke)
- 子图标签放在边框顶部

##### Scenario: 子图边界

- **WHEN** 子图包含 3 个节点
- **THEN** 边框 SHALL 包围所有节点
