# Spec: Mermaid2Excalidraw CLI Interface

## Purpose

为 Mermaid2Excalidraw 转换工具提供命令行接口规范。

## Requirements

### Requirement: CLI 命令行接口

`mermaid2excalidraw` 命令 SHALL 支持以下子命令：

- `convert`: 转换单个 Mermaid 文件
- `batch`: 批量转换目录
- `serve`: 启动预览服务器（可选）

#### Scenario: convert 命令用法

- **WHEN** 用户运行 `mermaid2excalidraw convert input.mmd -o output.excalidraw.json`
- **THEN** 将 input.mmd 转换为 Excalidraw JSON 并保存到 output.excalidraw.json

### Requirement: convert 命令参数

`convert` 命令 SHALL 支持以下参数：

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `input` | 输入 Mermaid 文件 | 必填 |
| `-o, --output` | 输出 Excalidraw 文件 | 必填 |
| `-s, --style` | 样式配置文件路径 | 使用默认样式 |
| `--width` | 画布宽度 | 800 |
| `--height` | 画布高度 | 600 |
| `--theme` | 主题 (light/dark) | light |

#### Scenario: 指定样式文件

- **WHEN** 用户运行 `mermaid2excalidraw convert input.mmd -o out.json -s custom.yml`
- **THEN** 使用 custom.yml 中的样式配置

### Requirement: batch 命令参数

`batch` 命令 SHALL 支持以下参数：

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `input_dir` | 输入目录 | 必填 |
| `-o, --output-dir` | 输出目录 | 必填 |
| `--pattern` | 文件匹配模式 | *.mmd |
| `-r, --recursive` | 递归处理子目录 | false |

#### Scenario: 批量转换

- **WHEN** 用户运行 `mermaid2excalidraw batch diagrams/ -o output/ -r`
- **THEN** 递归处理 diagrams/ 下所有 *.mmd 文件，输出到 output/
