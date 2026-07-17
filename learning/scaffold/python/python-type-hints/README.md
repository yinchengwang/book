# python-type-hints scaffold

Python 类型提示演示——TypeVar + Generic + Protocol + Union。

## 复现命令

```bash
cd learning/scaffold/python/python-type-hints
python3 main.py
```

## 预期输出

```
[1] 基础类型注解:
    greet('World'): Hello, World!
    process_numbers: total=15, avg=3.0

[2] 泛型类:
    int_stack.pop(): 2
    str_stack.pop(): world

[3] Protocol:
    绘制圆，半径=1.0
    绘制正方形，边长=2.0

[4] Union/Optional:
    parse_number('42'): 42
    parse_number('abc'): None

=== PASS ===
```

## 关键点

- **TypeVar**：泛型变量，如 `T = TypeVar('T')`
- **Generic**：泛型类继承，如 `class Stack(Generic[T])`
- **Protocol**：结构子类型，不需要显式继承
- **Union/Optional**：联合类型和可选类型

详见 NOTES.md。
