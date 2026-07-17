# virtual_env - Python 虚拟环境

## 概述

本卡演示 Python 虚拟环境的创建、管理和隔离原理。

## 核心概念

| 概念 | 说明 |
|------|------|
| venv | Python 3.3+ 内置模块，最简官方方案 |
| virtualenv | 第三方工具，更灵活，支持自定义 Python 版本 |
| site-packages | 包安装目录，虚拟环境隔离的核心 |
| VIRTUAL_ENV | 环境变量，标识当前虚拟环境 |

## 快速开始

```bash
# 运行演示
make run

# 或直接执行
bash main.sh
```

## 演示内容

1. **venv 模块** - Python 内置虚拟环境
2. **virtualenv 工具** - 第三方增强方案
3. **环境隔离** - site-packages 路径隔离原理

## 依赖

- Python 3.3+ (venv 模块)
- Bash 4.0+
- 可选: virtualenv 包 (`pip install virtualenv`)

## 验证

```bash
# 检查环境
make test

# 查看帮助
make help
```

## 常见问题

**Q: venv 和 virtualenv 如何选择？**
A: Python 3.3+ 推荐使用 venv，简单无依赖；需要自定义 Python 版本或更多特性时用 virtualenv。

**Q: 如何查看虚拟环境中的包？**
A: 激活环境后执行 `pip list` 即可。

**Q: 如何复制环境？**
A: 使用 `pip freeze > requirements.txt` 导出，`pip install -r requirements.txt` 导入。
