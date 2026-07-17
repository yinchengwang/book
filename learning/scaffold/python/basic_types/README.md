# basic_types scaffold

Python 基础类型演示——int/float/str/bool + list/dict/set/tuple + 类型转换 + id()/type() 内省 + 可变 vs 不可变对象。

## 复现命令

```bash
cd learning/scaffold/python/basic_types
python3 main.py
```

或使用 Makefile：

```bash
make run
```

## 预期输出（节选）

```
[types] === Python 基础类型 ===
  int    : 42 (type=int)
  float  : 3.14 (type=float)
  str    : 'Hello, Python!' (type=str)
  bool   : True (type=bool)

[compound] === 容器类型 ===
  list   : [1, 2, 3, 'a', 'b']
  dict   : {'name': 'Alice', 'age': 30, 'city': 'Beijing'}
  set    : {1, 2, 3} (自动去重)
  tuple  : (1, 2, 3, 'x') (不可变)

[cast] === 类型转换 ===
  int(3.7)   -> int   : 3 (截断小数)

[id_type] === id() 和 type() ===
  c=256, d=256（小整数池）:
    c is d: True（池内共享）

[mutable] === 可变 vs 不可变 ===
  list 可变: lst1=[1, 2, 3, 4], lst2=[1, 2, 3, 4]
    lst1 is lst2: True

=== PASS ===
```

## 关键点

- **int/float/str/bool** 是不可变类型，修改会创建新对象
- **list/dict/set** 是可变类型，原地修改会影响所有引用
- **小整数池**：-5~256 的整数共享同一对象（`is` 比较为 True）
- **类型转换**：`int(3.7)` 截断而非四舍五入，`bool()` 对所有"空值"返回 False
- **`id()`** 返回对象内存地址（CPython 实现），**`type()`** 返回类型对象

详见 NOTES.md 工程对照段。
