# python-dataclasses scaffold

Python dataclasses 演示——@dataclass + field + frozen。

## 复现命令

```bash
cd learning/scaffold/python/python-dataclasses
python3 main.py
```

## 预期输出

```
[1] dataclass 基础:
    Point: Point(x=1, y=2)
    Employee: Employee(name='Alice', salary=8000.0, active=True)

[2] field 定制:
    产品: Product(name='Apple', ...)
    库存总数: 2, 总价值: 8.0

[3] __post_init__:
    Person: Person(first_name='John', ...)
    full_name: John Doe

[4] 不可变:
    修改失败: cannot assign to field 'x' of ... ✓

=== PASS ===
```

## 关键点

- **@dataclass**：自动生成 __init__/__repr__/__eq__
- **field(default_factory=list)**：可变默认值
- **__post_init__**：初始化后处理
- **frozen=True**：不可变 dataclass

详见 NOTES.md。
