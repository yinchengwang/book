# ffi - Python FFI（ctypes + cffi）

## 概述

本卡演示 Python 与 C 代码的互操作机制。

## 核心概念

| 概念 | 说明 |
|------|------|
| ctypes | Python 标准库 FFI，加载共享库 |
| cffi | 第三方 FFI 库，更现代 |
| CDLL | 加载共享库的类 |
| restype | 指定 C 函数的返回类型 |
| argtypes | 指定 C 函数的参数类型 |

## 快速开始

```bash
make run
```

## 依赖

- Python 3.6+
- gcc（用于编译自定义 C 函数演示）
- 可选：cffi（`pip install cffi`）

## 验证

```bash
make test
```
