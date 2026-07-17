# R32 Python 工具链 — 规格规范

## 能力规格

| 规格 | 说明 |
|------|------|
| ID | r32-python-tools |
| 名称 | Python 工具链 |
| 类型 | 学习轨（learning） |
| 栈 | Python |
| 分层 | 工具链层 |

## 卡定义

| 卡 ID | 文件名前缀 | 演示内容 | 脚本语言 |
|-------|-----------|---------|---------|
| virtual_env | main.sh | venv/virtualenv 创建、激活、隔离原理 | bash |
| pip_packages | main.sh | pip install、requirements.txt、pip freeze | bash |
| logging | main.py | logging 模块、多 handler、配置 | python3 |
| packaging | main.sh | setup.py、pyproject.toml、打包 | bash |
| distribution | main.sh | wheel/sdist、PyPI、版本管理 | bash |
| c_extension | main.sh + cextension.c | Python C 扩展编译、CPython API | bash + C |
| ffi | main.py | ctypes、cffi 互操作 | python3 |
| fastapi | main.py | FastAPI 路由、Pydantic 验证 | python3 |
| numpy | main.py | ndarray、运算、矩阵 | python3 |
| pandas | main.py | DataFrame、筛选、聚合 | python3 |
| debugging | main.py | pdb、breakpoint、logging 调试 | python3 |

## 目录结构

```
learning/scaffold/python/<card-id>/
├── main.{sh,py}    # 演示主脚本
├── Makefile        # 构建/运行入口
├── README.md       # 快速开始
└── NOTES.md        # 学习笔记 + 工程对照
```

## 运行要求

| 卡 | 依赖 | 离线可运行 |
|----|------|-----------|
| virtual_env | python3, venv 模块 | 是 |
| pip_packages | pip | 是 |
| logging | python3 | 是 |
| packaging | setuptools | 是 |
| distribution | python3 | 是 |
| c_extension | python3-dev, gcc | 是 |
| ffi | gcc (部分) | 是 |
| fastapi | fastapi, uvicorn | 否 |
| numpy | numpy | 否 |
| pandas | pandas, numpy | 否 |
| debugging | python3 | 是 |
