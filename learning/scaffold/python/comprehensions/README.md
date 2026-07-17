# comprehensions scaffold

Python 推导式演示——list/dict/set 生成式 + 生成器表达式。

## 复现命令

```bash
cd learning/scaffold/python/comprehensions
python main.py
```

或使用 Makefile：

```bash
make run
```

## 预期输出（节选）

```
[list_comp] === 列表推导式 ===
  [x**2 for x in range(1,6)] -> [1, 4, 9, 16, 25]
  [x for x in 1..10 if even] -> [2, 4, 6, 8, 10]

[dict_comp] === 字典推导式 ===
  'hello' 字符计数: {'h': 1, 'e': 1, 'l': 2, 'o': 1}

[set_comp] === 集合推导式 ===
  去重平方: {1, 4, 9, 16}

[generator] === 生成器表达式 ===
  生成器对象: <generator object ...>

=== PASS ===
```

## 关键点

- **列表推导式**：`[expr for x in iter]`——可读性强，性能好
- **字典推导式**：`{k: v for k, v in items}`——快速构建字典
- **集合推导式**：`{expr for x in iter}`——自动去重
- **生成器表达式**：`gen = (expr for x in iter)`——惰性求值，内存高效
- **列表推导 vs map/filter**：推导式更 Pythonic，性能通常更好

详见 NOTES.md 工程对照段。
