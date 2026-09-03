# distribution 学习笔记

## 概念地图

Python 包分发是开源共享的关键环节：

- **wheel**：二进制分发，无需编译即装即用
- **sdist**：源码分发，需要编译环境
- **PyPI**：官方包仓库，`pip install` 的来源
- **twine**：安全上传工具

## 踩坑记录

1. **版本号冲突**：已发布的版本号不能重复使用
2. **包名冲突**：PyPI 包名必须唯一
3. **认证令牌泄露**：不要把 PyPI token 提交到代码库

## 工程对照（≥100 字硬约束）

### 1. 开源发布流程

Python 包发布的标准化流程：

```bash
# 1. 准备发布
git tag v1.0.0
# 2. 构建
python -m build
# 3. 上传
twine upload dist/*
```

### 2. 版本号管理

工程的版本号策略（参考项目规范）：

| 版本格式 | 用途 |
|---------|------|
| 1.0.0 | 正式发布 |
| 1.0.0a1 | alpha |
| 1.0.0b1 | beta |
| 1.0.0rc1 | 候选发布 |

### 3. PyPI 认证

安全的认证方式（使用 token）：

```bash
# ~/.pypirc 配置
[pypi]
username = __token__
password = pypi-xxxx
```

学完本卡能动手的事：将自己的 Python 工具发布到 TestPyPI。
