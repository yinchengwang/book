# python-slots scaffold

Python __slots__ 演示——内存优化 + 属性限制。

## 复现命令

```bash
cd learning/scaffold/python/python-slots
python3 main.py
```

## 预期输出

```
[1] __slots__ 基础:
    普通类实例 __dict__: {'name': 'regular', 'value': 1}
    Slotted 实例 __dict__: ...

[2] 内存对比:
    创建 100000 个实例:
    普通类: ~30 MB
    __slots__: ~8 MB
    节省: ~70%

=== PASS ===
```

## 关键点

- **__slots__**：限制实例可拥有的属性
- **内存节省**：避免 __dict__ 字典开销
- **属性限制**：无法动态添加新属性
- **与 property 配合**：使用 `_name` 存储

详见 NOTES.md。
