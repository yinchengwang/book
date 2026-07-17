# preprocessor scaffold

C 预处理器与宏演示——`#define` / `#` / `##` / `#ifdef` / `#pragma once` / `do-while(0)` / `__VA_ARGS__`。

## 复现命令

```bash
cd learning/scaffold/preprocessor

gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test

# 查看本机 GCC 预定义宏
gcc -E -dM - < /dev/null | head -20
```

## 预期输出（节选）

```
[define] === 对象宏 ===
  PI            = 3.141590

[func] === 函数宏 ===
  SQUARE(5)     = 25
  MAX(3,7)      = 7

[stringfy] === # 操作符 ===
  STR(hello)    = "hello"
  STR(PI)       = "PI"       # 注意：不展开为 "3.14..."

[concat] === ## 操作符 ===
  CONCAT(x,y)   = 42         # xy 变量

[ifdef] === 条件编译（按平台选） ===
  PLATFORM_NAME = "Windows"  # 或 "Linux" / "macOS"

[do-while] === do{ }while(0) 多语句宏 ===
  SWAP 前: x=10, y=20
  SWAP 后: x=20, y=10

[variadic] === __VA_ARGS__ ===
  [INFO] 系统启动, version=1.2, platform=Windows

=== PASS ===
```

## 关键点

- **函数宏必须外加括号**：`#define SQUARE(x) ((x)*(x))` 否则 `SQUARE(1+2)` 变 `1+2*1+2`
- **`#` 不展开宏**：`STR(PI)` 得到字符串 `"PI"` 而非 `"3.14..."`——需先用 `PI` 的值传递再 stringfy
- **`##` 拼接记号**：`CONCAT(x,y)` 把 `x` 和 `y` 拼成 `xy`，编译器查找该名字
- **`do{ }while(0)` 是宏的标准包装**：让多语句宏能在 `if/else` 中安全使用，不破坏语法
- **`__VA_ARGS__` + `##` 兼容 0 参**：`fprintf(stderr, fmt, ##__VA_ARGS__)` 在 GCC/Clang 中允许无参调用

详见 NOTES.md 工程对照段。