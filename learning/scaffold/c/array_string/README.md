# array_string scaffold

C 风格数组与字符串演示——一维/二维/VLA + 字符串字面量只读 + `<string.h>` 11 函数。

## 复现命令

```bash
cd learning/scaffold/array_string

gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
```

## 预期输出（节选）

```
[1d] === 一维数组 ===
  arr           = 20 bytes
  元素数         = 5

[2d] === 二维数组行主序布局 ===
  m 总字节       = 24 = 6 * 4
  线性遍历       = 1 2 3 4 5 6

[vla] === C99 变长数组 ===
  vla[4]        = 0 10 20 30
  vla 字节       = 16 (栈分配)

[str] === 字符串字面量只读 ===
  buf[0]='H' 后 = "Hello"
  p_lit         = "world" (只读段)

[funcs] === <string.h> 11 函数 ===
  strlen("Hello, World!") = 13
  strstr "World" = "World!"
  memmove       = "121234589" (重叠安全)

=== PASS ===
```

## 关键点

- **`sizeof(arr) / sizeof(arr[0])`** 是 C 语言求数组元素数的标准技巧（编译器警告时除外）
- **二维数组是连续内存**：行主序，`m[i][j]` 等价于 `*(m[0] + i*cols + j)`
- **VLA（C99）**：长度由运行时变量决定，在栈上分配。C11 标记为可选（`_Optional` feature）
- **`char[]` vs `char*`**：前者是数组（栈/全局，可写），后者是指针（可能指向只读段）
- **`strncpy` 不保证 `\0` 结尾**：必须手动补 `\0`，否则后续 `strlen` 越界

详见 NOTES.md 工程对照段。