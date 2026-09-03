# 单例模式

## 描述

本模块演示了单例模式 (Singleton Pattern) 的多种实现方式及应用场景。单例模式确保一个类只有一个实例, 并提供全局访问点。

本演示涵盖:
- **元类实现**: 通过自定义元类 `SingletonMeta` 实现线程安全的单例
- **装饰器实现**: 通过 `@singleton` 装饰器将任意类转换为单例
- **模块级单例**: Python 最推荐的方式 —— 模块天然是单例
- **`__new__` 实现**: 通过重写 `__new__` 方法实现单例

同时包含多线程安全性测试和数据库连接池的实际应用场景。

## 目录结构

```
pattern_singleton/
├── main.py    # 主演示文件
├── Makefile   # 构建/运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 如何运行

```bash
# 运行演示
make run

# 或者直接使用 python3
python3 main.py

# 检查环境并运行
make check

# 清理 (无产物需要清理)
make clean
```
