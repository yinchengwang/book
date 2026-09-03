# operator scaffold

C++ 运算符重载演示——成员/非成员/流/类型转换/explicit。

## 复现命令

```bash
cd learning/scaffold/operator

g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
```

> ⚠️ 运行时若被 `cin` 阻塞等待输入，请直接回车结束。

## 关键点

- **成员 vs 非成员**：
  - 一元运算符（`++`/`--`/`+=`/`[]`）通常成员
  - 二元运算符（`+`/`-`/`<<`）通常非成员（允许左操作数为 rvalue）
- **`friend` 用于非成员**：访问 private 数据时用 `friend Vec2 operator+(...)`
- **`<<` / `>>`**：必须是非成员，因为左操作数是 `ostream`/`istream`（不是你定义的类）
- **类型转换运算符**：`operator double() const` —— **加 `explicit` 防止意外隐式转换**
- **`explicit` 单参构造**：`Vec2(int)` 加 `explicit` 阻止 `v = 5` 隐式构造

详见 NOTES.md 工程对照段。