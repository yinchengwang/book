# python-closures scaffold

Python 闭包演示——嵌套函数 + nonlocal + 闭包工厂 + 私有变量。

## 复现命令

```bash
cd learning/scaffold/python/python-closures
python3 main.py
```

## 预期输出

```
[1] 基础闭包:
    outer(5)(3) = 8
    outer(10)(3) = 13

[2] 闭包工厂:
    double(7) = 14
    triple(7) = 21
    平方: 25
    立方: 8
    [INFO] 系统启动
    [INFO] 用户登录 admin

[3] nonlocal 修改外部变量:
    初始值: 10
    inc(5): 15
    inc(3): 18
    当前值: 18
    reset 后: 10

[4] 闭包实现私有变量（栈）:
    size=3, peek=3
    pop: 3
    pop: 2
    is_empty: False

[5] 闭包记忆化:
    fib(0) = 0
    fib(1) = 1
    fib(2) = 1
    ...
    缓存大小: 10

=== PASS ===
```

## 关键点

- **闭包**：内部函数引用外部作用域变量
- **__closure__**：存储闭包变量的单元格对象
- **nonlocal**：在嵌套函数中修改外部作用域变量
- **工厂函数**：返回闭包的函数，高阶函数模式

详见 NOTES.md。
