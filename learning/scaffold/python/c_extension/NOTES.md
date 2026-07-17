# c_extension 学习笔记

## 概念地图

Python C 扩展是性能关键代码的解决方案：

- **Python.h**：CPython 的 C API 接口
- **Extension**：setuptools 扩展模块配置
- **PyArg_ParseTuple**：从 Python 到 C 的参数解析
- **PyList_*/PyDict_***：C 层操作 Python 容器

## 踩坑记录

1. **Python 开发头文件**：需要安装 python3-dev/python3-devel
2. **GIL**：纯 C 代码需手动释放 GIL `Py_BEGIN_ALLOW_THREADS`
3. **内存管理**：C 扩展中的 malloc/free 必须配对

## 工程对照（≥100 字硬约束）

### 1. 项目中的 C 与 Python 互操作

本工程的 `learning/scripts/sync-pipeline.py` 是纯 Python 实现：

```python
# 如果性能瓶颈出现，可考虑用 C 扩展重写热点函数
# 例如文件哈希计算、正则匹配、JSON 序列化等
```

### 2. C 扩展与工程 C 代码的对比

本工程的 C 代码在 `engineering/src/` 中：

```c
// engineering/src/db/ 中的性能关键路径
// 数据库引擎用 C 实现，Python 层通过 C 扩展调用
```

### 3. 性能敏感度

| 场景 | 建议 |
|------|------|
| I/O 密集型 | Python 足够 |
| CPU 密集型 | C 扩展或 Cython |
| 数值计算 | NumPy / C 扩展 |
| 数据库引擎 | C/C++ 原生实现 |

学完本卡能动手的事：用 C 扩展为 Python 的热点函数加速。
