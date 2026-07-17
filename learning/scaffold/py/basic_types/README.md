# Python 基础类型

本模块演示 Python 核心内置类型的创建、转换、身份比较与运算符。

## 学习目标

1. 掌握 8 种基本内置类型的创建与 `type()` 查看方式
2. 理解类型转换规则（str ↔ bytes、数值 ↔ 字符串、序列 ↔ 字典）
3. 区分 `is`（身份比较）与 `==`（值比较）的语义差异
4. 熟悉算术、比较、逻辑、位运算、成员运算的用法
5. 了解类型注解（typing 模块）的基本写法

## 核心概念

### 类型注解

Python 3.5+ 支持类型注解（type hints），用于静态检查和文档：

```python
from typing import Dict, List, Set, Tuple

def process(data: List[int], meta: Dict[str, str]) -> Tuple[int, Set[str]]:
    total: int = sum(data)
    keys: Set[str] = set(meta.keys())
    return total, keys
```

### 基本类型一览

| 类型 | 字面量示例 | 可变 |
|------|-----------|------|
| `int` | `42` | - |
| `float` | `3.14` | - |
| `bool` | `True` | - |
| `str` | `"hello"` | - |
| `bytes` | `b"world"` | - |
| `list` | `[1, 2, 3]` | 是 |
| `tuple` | `(1, 2, 3)` | 否 |
| `dict` | `{"a": 1}` | 是 |
| `set` | `{1, 2, 3}` | 是 |

### 类型转换

| 转换 | 示例 |
|------|------|
| `int("100")` | 字符串 → 整数 |
| `str(42)` | 整数 → 字符串 |
| `list((1,2,3))` | 元组 → 列表 |
| `dict([("k","v")])` | 键值对列表 → 字典 |
| `b"abc".decode()` | bytes → str |
| `"abc".encode()` | str → bytes |

### is vs ==

- **`is`**：检查两个变量是否指向同一对象（身份比较）
- **`==`**：检查两个变量的值是否相等（值比较）

特殊情形：
- `None` 是单例，`None is None` 为 True
- 小整数 -5~256 被缓存复用，`100 is 100` 为 True
- 大整数每次新建对象，`1000 is 1000` 为 False
- 字符串字面量会被驻留（interning）

## 编译运行方式

### 方式 1：Make（推荐）

```bash
cd learning/scaffold/py/basic_types
make check
```

### 方式 2：直接运行

```bash
cd learning/scaffold/py/basic_types
python3 main.py
```

预期输出：`=== PASS ===`，退出码 0。

## 进阶阅读

- [PEP 484 — Type Hints](https://peps.python.org/pep-0484/)
- [Python 官方文档：内置类型](https://docs.python.org/3/library/stdtypes.html)
