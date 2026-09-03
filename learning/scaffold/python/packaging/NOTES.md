# packaging 学习笔记

## 概念地图

Python 打包经历了从 setup.py 到 pyproject.toml 的演进：

- **pyproject.toml**：现代标准（PEP 517/518），声明式配置
- **setup.py**：传统方式，命令式脚本
- **setuptools**：最常用的打包后端
- **build**：独立构建工具，`pip install build`

## 踩坑记录

1. **pyproject.toml 格式错误**：必须包含 `[build-system]` 和 `[project]` 部分
2. **包找不到**：检查 `packages.find` 配置或 `find_packages()` 返回值
3. **中文路径问题**：包路径避免中文字符

## 工程对照（≥100 字硬约束）

### 1. 项目打包实践

本项目的 Python 脚本通过 setuptools 打包：

```python
# learning/scripts/setup.py（如果存在）
from setuptools import setup, find_packages
setup(
    name="book-scripts",
    version="1.0.0",
    packages=find_packages(),
)
```

### 2. 工程中的 Python 依赖管理

工程的 requirements.txt 与打包协同工作：

```bash
# 构建时安装依赖
pip install -e .        # 开发模式安装
pip install .           # 普通安装
pip install --wheel .   # 仅构建 wheel
```

### 3. 多平台打包注意事项

| 场景 | 建议 |
|------|------|
| 纯 Python 包 | pyproject.toml + wheel |
| C 扩展包 | setup.py + 条件编译 |
| 跨平台 | setup.py 或 pyproject.toml |

学完本卡能动手的事：为自己的 Python 项目创建 pyproject.toml，发布到 PyPI。
