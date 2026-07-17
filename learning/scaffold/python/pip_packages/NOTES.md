# pip_packages 学习笔记

## 概念地图

pip 是 Python 的官方包管理器，贯穿项目的依赖管理实践：

- **pip install**：安装单个或批量包
- **requirements.txt**：依赖清单，标准化团队开发环境
- **pip freeze**：导出精确版本，便于复现
- **pip check**：检查依赖冲突，避免运行时问题

## 踩坑记录

1. **pip 版本过旧**：`pip install --upgrade pip` 保持最新
2. **权限问题**：避免 `sudo pip`，用虚拟环境代替
3. **镜像源配置错误**：国内用户需配置清华/阿里镜像
4. **pip check 失败**：依赖冲突会导致运行时诡异的 bug

## 工程对照（≥100 字硬约束）

pip 在本项目的工程实践中多处应用：

### 1. sync-pipeline.py 的依赖管理

`sync-pipeline.py` 依赖 requests、PyYAML、tqdm 等库：

```bash
# learning/scripts/ 中的依赖安装方式
$ pip install requests PyYAML tqdm
$ pip freeze > requirements.txt  # 锁定版本
```

### 2. GitHub Actions CI 中的 pip 用法

项目的 CI 配置（`.github/workflows/`）中：

```yaml
# CI 流程中的 Python 依赖安装
- name: Install dependencies
  run: |
    python -m venv .venv
    .venv/bin/pip install --upgrade pip
    .venv/bin/pip install -r requirements.txt
```

### 3. 分层依赖管理实践

工程中常见的多层 requirements：

| 文件 | 用途 | 包含内容 |
|------|------|---------|
| requirements.txt | 核心依赖 | requests, PyYAML |
| requirements-dev.txt | 开发依赖 | -r requirements.txt, pytest, black |
| requirements-test.txt | 测试依赖 | -r requirements-dev.txt, coverage |

### 4. pip cache 加速 CI

```yaml
# 利用 pip 缓存加速 CI
- name: Cache pip packages
  uses: actions/cache@v3
  with:
    path: ~/.cache/pip
    key: ${{ runner.os }}-pip-${{ hashFiles('requirements.txt') }}
```

学完本卡能动手的事：为自己的 Python 项目编写 requirements.txt，用 `pip freeze` 锁定依赖版本。
