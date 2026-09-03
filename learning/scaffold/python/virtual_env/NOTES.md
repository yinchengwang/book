# virtual_env 学习笔记

## 概念地图

Python 虚拟环境是解决"依赖冲突"问题的标准方案：

- **venv**：Python 3.3+ 内置模块，创建轻量级虚拟环境
- **virtualenv**：第三方工具，支持 Python 2/3，自定义 Python 版本
- **环境隔离**：每个虚拟环境有独立的 site-packages、pip、Python 解释器
- **VIRTUAL_ENV**：环境变量，激活时自动设置，deactivate 时清除

## 踩坑记录

1. **Windows 激活路径**：Windows 用 `venv\Scripts\activate`，不是 `venv/bin/activate`
2. **嵌套虚拟环境**：不要在虚拟环境中再创建虚拟环境，会导致路径混乱
3. **pip 版本过旧**：新环境可能需要 `pip install --upgrade pip`
4. **删除虚拟环境**：直接 `rm -rf venv` 即可，无需 uninstall

## 工程对照（≥100 字硬约束）

Python 虚拟环境在本项目的工程实践中有多处应用：

### 1. sync-pipeline.py 的依赖隔离实践

`sync-pipeline.py` 依赖 requests、PyYAML 等库，在实际部署时使用虚拟环境隔离：

```python
# learning/scripts/sync-pipeline.py 的典型部署方式
# $ python3 -m venv .venv
# $ source .venv/bin/activate
# $ pip install requests PyYAML tqdm
# $ python sync-pipeline.py ...
```

### 2. learning/ 和 engineering/ 的 Python 脚本隔离

项目中的 Python 脚本分布在多个目录，通过虚拟环境实现依赖隔离：

| 脚本 | 位置 | 依赖 |
|------|------|------|
| sync-pipeline.py | learning/scripts/ | requests, PyYAML, tqdm |
| reading-radar | engineering/apps/web/ | Node.js 生态 |

### 3. 虚拟环境在 CI/CD 中的应用

项目的 GitHub Actions（`.github/workflows/`）中：

```yaml
# CI 流程中的 Python 环境设置
- name: Set up Python
  uses: actions/setup-python@v4
  with:
    python-version: '3.11'

- name: Create venv
  run: python -m venv .venv

- name: Install dependencies
  run: .venv/bin/pip install -r requirements.txt
```

### 4. requirements.txt 的版本锁定实践

工程实践中通过 requirements.txt 锁定依赖版本：

```bash
# 导出当前环境依赖
pip freeze > requirements.txt

# 在新环境安装
pip install -r requirements.txt
```

**关键原则**：开发环境用 `requirements.txt` 记录依赖，生产环境用 `requirements-prod.txt`（不含开发依赖）。

学完本卡能动手的事：为自己的 Python 项目创建虚拟环境，用 requirements.txt 管理依赖。
