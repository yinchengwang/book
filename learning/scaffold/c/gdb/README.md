# gdb scaffold

故意埋三个 bug 的小程序。`gcc -g -O0` 编译后用 gdb 找出来。

## 复现命令

本机 MinGW-w64 实测可跑（无 ASAN）：

```bash
cd learning/scaffold/gdb
gcc -g -O0 -Wall -Wextra -std=c11 -o buggy main.c

# 正常路径——能跑（但有 Bug B 隐患）
./buggy "Hello World"

# 触发 Bug C：空指针 strlen → 段错误
./buggy
# 输出: Segmentation fault (bash 报)
```

## 三个 Bug

- **Bug A** `is_vowel()` 用 `sizeof(VOWELS)` 在文件作用域常量上是 10 字节，但语义模糊——读者应用 `VOWELS_SIZE` 宏
- **Bug B** `print_vowel_table` 用 `s[i]-'a'` 做 idx，越界实际靠 `if (idx >= 0 && idx < VOWELS_SIZE)` 钳制保留；assert 加固会更安全
- **Bug C** **未检查 NULL 直接 strlen**——空参数触发段错误

## gdb 调试步骤

```bash
gdb ./buggy
> run               # 触发 crash
> bt                # 查看 crash 在哪一行（top frame 的 line）
> frame 0           # 进最深的栈帧
> print len         # 看 len 变量，期望是错误值
> list main.c:48    # 看 main 第 48 行附近
> info locals       # 看所有局部变量
> quit
```

或 gdb 启动时 -batch 自动跑：

```bash
gdb -batch -ex 'run' -ex 'bt' -ex 'info locals' ./buggy
```

## 关键点

- **必须 `-g`**：生成调试符号；`-O0` 防止编译器把变量优化掉
- **`bt` 看栈**：立即定位"哪个函数哪一行 crash"
- **`info locals` 看变量**：错误参数 / NULL 指针一目了然
- **`watch` 监控变量变化**：运行时未到 crash 的 bug 用这个
- **`ASAN` 更友好**：直接定位越界与 use-after-free；本机 mingw 没 ASAN lib，所以本 demo 用纯 gdb 路径

详见 NOTES.md 工程对照段。
