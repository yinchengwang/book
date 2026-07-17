# python-generator scaffold

Python 生成器演示——yield + 生成器表达式 + itertools 应用。

## 复现命令

```bash
cd learning/scaffold/python/python-generator
python3 main.py
```

## 预期输出

```
[1] 基础生成器:
    count_up_to(5): [1, 2, 3, 4, 5]
    fibonacci_generator(10): [0, 1, 1, 2, 3, 5, 8, 13, 21, 34]
    primes_generator(50): [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47]

[2] 生成器表达式:
    列表推导式: [0, 1, 4, 9, 16, 25, 36, 49, 64, 81]
    生成器表达式: <generator object ...>
    next(): 0
    next(): 1
    list(): [4, 9, 16, 25, 36, 49, 64, 81]

[3] itertools 应用:
    count(10,2): 前5个 [10, 12, 14, 16, 18]
    cycle: 前7个 ['A', 'B', 'C', 'A', 'B', 'C', 'A']
    accumulate: [1, 3, 6, 10, 15]
    chain: [1, 2, 'a', 'b', 3, 4]

[4] 生成器管道:
    管道 [1..20] -> 过滤偶数 -> 平方 -> 取前5: [4, 8, 12, 16, 20]

[5] 协程风格（send）:
    收到: Hello
    收到: 42
    收到: [1, 2, 3]

[6] yield from（委托生成器）:
    yield from: [3, 1, 2, 4]

=== PASS ===
```

## 关键点

- **yield**：函数遇到 yield 暂停执行，返回值并保留状态
- **生成器表达式**：(x for x in iter) 惰性求值，内存友好
- **itertools**：count/cycle/chain/groupby/permutations 等工具
- **管道**：filter -> map -> islice 组合处理大数据集

详见 NOTES.md。
