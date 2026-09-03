# c_extension - Python C 扩展

## 概述

本卡演示 Python C 扩展的编写、编译和调用。

## 核心概念

| 概念 | 说明 |
|------|------|
| Python.h | CPython C API 头文件 |
| Extension | setuptools 扩展模块 |
| PyArg_ParseTuple | 参数解析 |
| PyMODINIT_FUNC | 模块初始化函数 |

## 快速开始

```bash
make run
```

## 注意

需要 Python 开发头文件（python3-dev）。
