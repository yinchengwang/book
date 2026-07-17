# class_object scaffold

C++ 类与对象演示——构造函数/析构/初始化列表/const/static/this/friend。

## 复现命令

```bash
cd learning/scaffold/class_object

g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
```

## 预期输出（节选）

```
[ctor] === 构造函数与析构 ===
  [ctor] Point() default at 0x...
  [ctor] Point(3.0, 4.0)
  Point 实例数 = 2

[initlist] === 初始化列表 vs 体内赋值 ===
  p4 (initlist): Point(5.00, 6.00)

[const] === const 成员函数 ===
  const Point: x=7.00, y=8.00

[static] === static 成员（类级共享）===
  当前实例数 = 4

[this] === this 指针与链式调用 ===
  p5 after 3 shifts: Point(6.00, 6.00)

[friend] === friend 友元类 ===
  pair2 manhattan: distance = 7.00

=== PASS ===
```

## 关键点

- **构造函数**：默认 / 参数化 / 拷贝（C++11 还有移动）
- **初始化列表**：`Point(double x, double y) : x_(x), y_(y)` ——const/引用成员必须用
- **`const` 成员函数**：承诺不修改状态，const 对象只能调 const 函数
- **`static` 成员**：类级共享，所有对象一份；定义在类外
- **`this` 指针**：指向当前对象的隐式指针；链式调用返回 `*this`
- **`friend` 友元**：破坏封装的"合法渠道"——友元类/函数可访问 private

详见 NOTES.md 工程对照段。