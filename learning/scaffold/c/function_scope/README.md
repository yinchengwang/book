# function_scope scaffold

C 函数与作用域演示——prototype/传值传址/static/extern/块作用域。

## 复现命令

```bash
cd learning/scaffold/function_scope

gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
```

## 预期输出（节选）

```
[proto] === 函数声明（prototype）与定义 ===
  add(3, 4) = 7 (call_count=1)

[param] === 传值 vs 传址 ===
  传值前: a=10, b=20
  手动 swap: a=20, b=10
  swap(&a,&b): a=10, b=20

[static] === static 函数/变量 ===
  add(0, 1) = 1
  add(1, 2) = 3
  add(2, 3) = 5
  call_count = 4

[extern] === extern 跨文件引用 ===
  PROGRAM_NAME = "function_scope"

[block] === 块作用域与文件作用域 ===
  outer = 100 (main 作用域)
  inner = 200 (内层块)
  inner outer = 999 (内层遮蔽外层 outer)
  outer = 100 (离开内层块，外层 outer 恢复)
  counter_increment 三次: 1, 2, 3

=== PASS ===
```

## 关键点

- **prototype 让编译器检查调用约定**：返回值类型 + 参数类型 + 参数个数 — 调用不匹配会编译警告
- **传值 vs 传址**：基本类型传值（拷贝），struct/数组传址（指针）——C 没有引用类型
- **static 函数**：仅本 `.c` 文件可见，**避免命名冲突** + **编译器可内联**
- **static 全局变量**：仅本文件可见，**避免外部修改**（封装性）
- **static 局部变量**：生命周期 = 程序周期，但作用域仍是函数内
- **extern**：声明"变量在别的 .c 文件定义"，不分配内存
- **块作用域**：内层 `int x = 999` 遮蔽外层 `x`，离开块后恢复

详见 NOTES.md 工程对照段。