# pointer scaffold

指针四件核心：地址、解引用、指针算术、函数指针。

## 复现命令

```bash
cd learning/scaffold/pointer
gcc -Wall -Wextra -O2 -std=c11 -o pointer_demo main.c
./pointer_demo
```

## 预期输出

```
[ptr]    x   = 42 (addr=...)
[ptr]    *p  = 42 (p=...)
[ptr]    after *p=100, x = 100
[arith]  arr[2] via *(q+2) = 30
[arith]  arr[2] via q[2]   = 30
[fnptr]  add(6, 4) = 10
[fnptr]  sub(6, 4) = 2
[fnptr]  mul(6, 4) = 24
[null]  np == NULL, skip deref
```

## 关键点

- `*p = 100` 反向修改原值（指针是间接访问入口）
- `*(q+2)` 与 `q[2]` 等价，编译器翻译相同 SSA
- 函数指针数组 → 调度表（dispatch table）模式
- NULL 防御：每次拿到外部指针必查 NULL

详见 NOTES.md 工程对照段。
