# functions 学习笔记

## 概念地图

Python 函数是"第一类对象"——可以赋值给变量、作为参数传递、作为返回值：

- **位置参数 vs 关键字参数**：位置参数按顺序匹配，关键字参数按名称匹配
- **默认参数**：定义时求值，**禁止使用可变对象作为默认参数**（经典陷阱）
- **`*args`/`**kwargs`**：接收任意数量的参数，实现可变参数 API
- **lambda 表达式**：单行匿名函数，适合简短回调
- **闭包**：内部函数引用外部作用域变量，捕获"自由变量"
- **高阶函数**：`map()`/`filter()`/`reduce()` 是函数式编程基础

## 踩坑记录

1. **默认参数的可变陷阱**：`def add(item, bag=[])` 每次调用共享同一个列表，用 `bag=None` + `if bag is None: bag = []`
2. **闭包延迟绑定**：循环中创建闭包时，变量是延迟绑定的——用默认参数捕获当前值
3. **`*args` 展开顺序**：`*args` 必须放在位置参数后，`**kwargs` 放在最后
4. **lambda 限制**：lambda 体只能是单个表达式，不能包含语句
5. **reduce 的初始值**：必须提供初始值 `reduce(f, seq, init)`，否则空序列报错

## 工程对照（≥100 字硬约束）

Python 函数式编程在 `learning/scripts/sync-pipeline.py` 中有大量实践：

1. **lambda 与 map/filter**：`sync-pipeline.py` 中 `files = list(filter(lambda f: not f.startswith('.'), paths))` 过滤隐藏文件
2. **函数作为返回值**：装饰器模式中 `make_multiplier(2)` 返回一个乘以 2 的函数，类似 `functools.lru_cache` 的实现
3. **`*args` 解包**：`print(*args)` 将元组展开为位置参数，`print(**kwargs)` 展开为关键字参数
4. **`**kwargs` 配置传递**：许多 Web 框架（Flask/FastAPI）用 `**kwargs` 传递额外配置参数
5. **默认参数配置**：`def query(sql, timeout=30, pool_size=10)` 提供合理的默认配置
6. **闭包实现记忆化**：`@lru_cache` 本质是闭包，存储参数到结果的映射

学完本卡能动手的事：写一个 `retry(max_attempts, delay)` 装饰器，实现指数退避重试逻辑。
