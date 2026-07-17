# functions scaffold

Python 函数演示——def/lambda + 默认参数 + *args/**kwargs + 闭包 + map/filter/reduce。

## 复现命令

```bash
cd learning/scaffold/python/functions
python main.py
```

或使用 Makefile：

```bash
make run
```

## 预期输出（节选）

```
[def] === 函数定义与调用 ===
  greet('Alice') -> 'Hello, Alice!'
  divide(10, 3) -> quotient=3, remainder=1

[default] === 默认参数 ===
  connect('localhost')         -> postgresql://localhost:5432 (timeout=30s)
  append_to(1) -> [1]
  append_to(2) -> [1, 2]  (陷阱！)

[args] === *args 可变位置参数 ===
  sum_all(1, 2, 3) -> 6
  sum_all(*numbers) -> 15

[closure] === 闭包 ===
  double(5) -> 10
  triple(5) -> 15

[builtin] === map/filter/reduce ===
  map(x**2, 1..10) -> [1, 4, 9, 16, 25, 36, 49, 64, 81, 100]
  filter(even, 1..10) -> [2, 4, 6, 8, 10]
  sum of squares of evens -> 220

=== PASS ===
```

## 关键点

- **默认参数陷阱**：`def f(x=[])` 会导致列表在调用间共享，用 `def f(x=None)` + `if x is None: x = []`
- **闭包捕获变量**：内部函数引用外部变量，用 `nonlocal` 修改变量
- **`*args`/`**kwargs`**：接收任意数量的参数，`*` 解包序列，`**` 解包字典
- **lambda 表达式**：单行匿名函数，常与 map/filter/sorted 结合

详见 NOTES.md 工程对照段。
