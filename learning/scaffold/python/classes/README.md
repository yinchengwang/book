# classes scaffold

Python 面向对象演示——class/__init__/继承 + 多态 + 特殊方法 + @property + @classmethod/@staticmethod + MRO。

## 复现命令

```bash
cd learning/scaffold/python/classes
python main.py
```

或使用 Makefile：

```bash
make run
```

## 预期输出（节选）

```
[class_def] === 类定义与 __init__ ===
  alice.greet() -> 'Hello, I'm Alice'
  alice -> Person(Alice, 30)

[inheritance] === 继承与多态 ===
  student.greet() -> 'Hi, I'm Charlie, a Junior student'
  isinstance(student, Person) -> True

[magic] === 特殊方法 ===
  v1 + v2 -> Vector(4.0, 6.0)
  v1 * 2 -> Vector(2.0, 4.0)

[property] === @property 装饰器 ===
  circle.area -> 78.54

[classmethod] === @classmethod vs @staticmethod ===
  Date.is_valid_date(2024, 2, 29) -> True
  Date.from_string('2024-01-15') -> 2024-01-15

[diamond] === 多重继承与 MRO ===
  D().who() -> D
  MRO of D -> ['D', 'B', 'C', 'A', 'object']

=== PASS ===
```

## 关键点

- **`__init__`**：构造函数，创建对象时初始化实例属性
- **`super().__init__()`**：调用父类构造函数，避免重复初始化
- **多态**：子类重写父类方法，调用时根据实际类型执行
- **`__str__`/`__repr__`**：控制对象的字符串表示
- **`@property`**：将方法转换为只读属性，实现封装
- **MRO（方法解析顺序）**：多重继承时，Python 使用 C3 线性化算法决定方法查找顺序

详见 NOTES.md 工程对照段。
