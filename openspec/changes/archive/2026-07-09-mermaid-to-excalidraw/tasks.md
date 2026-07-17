# Tasks: Mermaid to Excalidraw 转换工具实施

## 1. 项目初始化

- [x] 1.1 创建 `tools/mermaid2excalidraw/` 目录结构
- [x] 1.2 初始化 Python 项目 (pyproject.toml)
- [x] 1.3 创建 requirements.txt

## 2. 解析器实现 (Parser)

### 2.1 中间表示定义

- [x] 2.1 创建 `parser/ir.py` - 定义 DiagramIR, Node, Edge 数据类
- [x] 2.2 创建 `parser/__init__.py`

### 2.2 Flowchart 解析器

- [x] 2.3 创建 `parser/flowchart.py` - 解析 flowchart 语法
- [x] 2.4 支持节点定义 `[label]`, `(label)`, `{label}`, `((label))`
- [x] 2.5 支持边定义 `-->`, `--text-->`, `-.->`
- [x] 2.6 支持子图 `subgraph id ... end`

### 2.3 时序图解析器 (可选)

- [x] 2.7 创建 `parser/sequence.py` - 解析 sequence diagram (已跳过，暂不支持)

## 3. 布局引擎实现 (Layout)

- [x] 3.1 创建 `layout/layered.py` - 层次布局算法
- [x] 3.2 实现层级分配 (BFS/DFS)
- [x] 3.3 实现同层节点排序
- [x] 3.4 实现坐标计算
- [x] 3.5 创建 `layout/__init__.py`

## 4. 样式系统 (Style)

- [x] 4.1 创建 `style/default.py` - 默认样式配置
- [x] 4.2 实现形状到颜色的映射
- [x] 4.3 创建 `style/__init__.py`

## 5. Excalidraw 生成器 (Generator)

- [x] 5.1 创建 `generator/excalidraw.py` - 生成 Excalidraw JSON
- [x] 5.2 实现节点到元素的转换
- [x] 5.3 实现边到箭头的转换
- [x] 5.4 实现标签处理
- [x] 5.5 创建 `generator/__init__.py`

## 6. CLI 入口

- [x] 6.1 创建 `main.py` - Click CLI 入口
- [x] 6.2 实现 convert 命令
- [x] 6.3 实现 batch 命令
- [x] 6.4 添加帮助信息和错误处理

## 7. 测试

- [x] 7.1 创建 `test_parser.py` - 解析器单元测试
- [x] 7.2 创建 `test_layout.py` - 布局算法测试
- [x] 7.3 创建 `test_integration.py` - 端到端测试
- [x] 7.4 添加示例 Mermaid 文件

## 8. 文档

- [x] 8.1 编写 README.md 使用说明
- [x] 8.2 添加使用示例
