# ffi 学习笔记

## 概念地图

Python FFI 是 Python 与 C 代码互操作的核心技术：

- **ctypes**：标准库，无需编译，加载共享库
- **cffi**：第三方库，更现代，类型更安全
- **ABI**：应用程序二进制接口

## 踩坑记录

1. **restype**：ctypes 默认返回 c_int，其他类型需显式指定
2. **argtypes**：指定参数类型可获得自动类型转换和错误检查
3. **字符串**：ctypes 默认是 bytes，需 decode
4. **内存所有权**：ctypes 不管理 C 分配的内存

## 工程对照（≥100 字硬约束）

### 1. 工程中的 C 与 Python 交互

本项目主体是 C/C++ 代码（`engineering/src/`）和 Python 工具脚本（`learning/scripts/`），FFI 是语言互操作的关键：

```python
# 如果工程的 C 库需要 Python 调用，可用 ctypes
# 例如调用数据库引擎中的函数进行测试
from ctypes import CDLL
db_lib = CDLL('./build/engineering/libdb.so')
```

### 2. 与 C 扩展的对比

| 方案 | 编译 | 性能 | 难度 |
|------|------|------|------|
| ctypes | 不需编译 | 有调用开销 | 低 |
| cffi | 可选编译 | 较好 | 中 |
| C 扩展 | 需编译 | 最高 | 高 |

### 3. 跨平台注意事项

```python
# 不同平台加载方式不同
if sys.platform == 'win32':
    lib = CDLL('msvcrt.dll')
elif sys.platform == 'linux':
    lib = CDLL('libc.so.6')
```

学完本卡能动手的事：用 ctypes 加载工程编译的 .so/.dll，从 Python 调用 C 函数。
