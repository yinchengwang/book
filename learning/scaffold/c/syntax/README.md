# syntax scaffold

C 语言数据类型与运算符演示——12 种类型 size 验证 + 运算符优先级表 + 整数边界值 + 类型转换。

## 复现命令

```bash
cd learning/scaffold/syntax

gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
```

## 预期输出（节选）

```
[types] === 12 种 C 类型大小 ===
  char           : 1 byte
  int            : 4 byte
  long           : 4 byte  # MinGW Windows; Linux 64-bit 上是 8
  int64_t        : 8 byte
  ...

[ops] === 运算符优先级演示 ===
  1 + 2 * 3     = 7 (乘优先)
  1 << 2 + 1    = 8 (<< 优先于 +)
  flags&MASK==0 = 0 (== 优先于 &，常见坑)

[bounds] === 整数类型边界 ===
  INT_MAX       = 2147483647
  UINT_MAX      = 4294967295
  UINT_MAX+1    = 0 (无符号环绕)

[cast] === 类型转换 ===
  int 42 → double 42.000000 (隐式，安全)
  double 3.7 → int 3 (隐式，截断)

=== PASS ===
```

## 关键点

- **`sizeof` 返回 `size_t`**（无符号整数），用 `%zu` 格式化——这是 C99 之后的标准做法
- **运算符优先级口诀**：括号 > 单目 > 乘除 > 加减 > 移位 > 关系 > 位 > 逻辑 > 赋值 > 逗号
- **`flags & MASK == 0` 是经典坑**：等价于 `flags & (MASK == 0)`，永远少写括号
- **有符号溢出是 UB**（编译器可做任何事）；**无符号溢出环绕**（标准定义行为）

详见 NOTES.md 工程对照段。