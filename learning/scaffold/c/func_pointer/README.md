# func_pointer scaffold

函数指针的四件应用：qsort 回调、调度表、atexit 清理钩子（与 signal §6 互引 sigaction handler）。

## 复现

```bash
cd learning/scaffold/func_pointer
gcc -Wall -Wextra -O2 -std=c11 -o funcptr_demo main.c
./funcptr_demo
```

## 预期输出

```
[qsort]  asc  : 1 2 3 4 5 7 8 9
[qsort]  desc : 9 8 7 5 4 3 2 1
[arith]  add(3, 4) = 7
[arith]  mul(3, 4) = 12
[atexit] cleanup hook fired
```

注意输出顺序：`atexit_clean` 在 `main return` 后打印，所以出现在 stdout 之后。

## 关键点

- **qsort 回调签名必须是 `(const void *, const void *)`**：因为 qsort 不知道你要排什么类型
- **typedef 函数指针**：`typedef int (*arith_t)(int, int);` 比裸声明 `int (*)(int, int)` 易读
- **atexit LIFO**：后注册先回调，可注册多个用作多阶段清理（与 §6 signal 互引——signal handler 注册同理但顺序无关）
- **函数指针陷阱**：调用前必须非 NULL；`if (fp) fp();` 是标配

详见 NOTES.md 工程对照段。
