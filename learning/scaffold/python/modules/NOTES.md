# modules 学习笔记

## 概念地图

Python 的模块系统是代码组织的核心：

- **模块**：单个 `.py` 文件，顶层变量/函数/类组成模块属性
- **包**：包含 `__init__.py` 的目录，用于组织多个模块
- **`import` 语义**：将模块加载到 `sys.modules`，并将模块名添加到局部命名空间
- **`from import` 语义**：直接从模块复制指定符号到局部命名空间
- **`__all__`**：控制 `from module import *` 的行为，提高接口清晰度
- **模块只加载一次**：多次 `import` 不会重复执行模块代码

## 踩坑记录

1. **循环导入**：`a.py` 导入 `b.py`，`b.py` 又导入 `a.py` 会导致导入顺序问题
2. **`from module import *`**：导入所有非下划线开头的符号，可能覆盖已有名称
3. **相对导入**：包内使用 `from . import sibling`，`..` 表示父包
4. **模块搜索顺序**：当前目录优先于标准库，可能意外覆盖内置模块
5. **`__name__` 判断**：只在直接执行时为 `__main__`，测试时常用

## 工程对照（≥100 字硬约束）

Python 模块化设计在 `learning/scripts/` 和 `engineering/` 中有大量实践：

1. **模块划分**：`learning/scripts/sync-pipeline.py` 作为入口，导入 `utils`、`config` 等子模块
2. **`__all__` 控制导出**：`engineering/src/db/` 的 C 代码没有 `__all__`，但 Python 库（Flask/SQLAlchemy）用它控制公开 API
3. **相对导入**：Flask 的 `from flask import Flask` 是绝对导入，`from . import views` 是相对导入
4. **`sys.path` 动态添加**：`PYTHONPATH` 环境变量或 `sys.path.insert(0, path)` 添加自定义路径
5. **包初始化**：`__init__.py` 可以设置 `__all__`、导入子模块、配置包级别变量
6. **单例模式**：数据库连接池、配置对象常作为模块级单例

学完本卡能动手的事：创建一个小型的工具包，包含 `string_utils.py`、`date_utils.py`，用 `__all__` 控制导出。
