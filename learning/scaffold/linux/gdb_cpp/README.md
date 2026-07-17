# GDB C++ 调试指南

## 编译要求

```bash
# 编译含 bug 版本（启用 ENABLE_BUG）
g++ -std=c++17 -g -O0 -DENABLE_BUG main.cpp -o main_bug

# 编译修复版本
g++ -std=c++17 -g -O0 main.cpp -o main_fixed
```

- `-g`: 保留调试符号
- `-O0`: 禁用优化（防止代码被重排影响调试）
- `-std=c++17`: 使用 C++17 标准

## GDB 常用命令速查

| 命令 | 作用 | 示例 |
|------|------|------|
| `break` / `b` | 设置断点 | `break main.cpp:25` |
| `run` / `r` | 运行程序 | `run` 或 `run arg1 arg2` |
| `next` / `n` | 单步执行（不进入函数） | `next` |
| `step` / `s` | 单步执行（进入函数） | `step` |
| `finish` / `fin` | 执行到函数返回 | `finish` |
| `continue` / `c` | 继续运行 | `continue` |
| `print` / `p` | 打印变量 | `print counter` |
| `watch` | 设置监视点 | `watch global_counter` |
| `backtrace` / `bt` | 显示调用栈 | `bt` |
| `info threads` | 查看线程 | `info threads` |
| `frame` / `f` | 切换栈帧 | `frame 1` |
| `disas` | 反汇编 | `disas` |
| `condition` | 设置条件断点 | `condition 2 counter > 100` |

## 条件断点

当特定条件满足时才停住，适合定位特定数据状态的 bug。

```bash
# 设置条件断点
break main.cpp:25 if counter > 100

# 为已有断点添加条件
condition 2 counter == 101

# 移除条件
condition 2

# 忽略前 N 次触发
ignore 3 10    # 忽略断点 3 前 10 次触发
```

### 示例：定位计数器 bug

```bash
$ gdb ./main_bug
(gdb) break main.cpp:45 if global_counter > 10
(gdb) run
# 程序在 global_counter 超过 10 时停下
(gdb) print global_counter    # 观察值是否异常
(gdb) step                    # 单步看 ++ 操作
```

## Watch Point（监视点）

监视变量值变化，在变量被修改时自动停下。

```bash
# 监视全局变量
watch global_counter

# 监视指针指向的值
watch *ptr

# 读监视（变量被读时停住）
rwatch global_counter

# 访问监视（变量被读写时停住）
awatch global_counter

# 查看所有监视点
info watchpoints
```

### 示例：定位竞态条件

```bash
(gdb) break race_condition_demo
(gdb) run
(gdb) watch shared_value
(gdb) continue
# 每次 shared_value 被修改都会停下
# 可以观察 read-modify-write 是否被打断
```

## 调试优化代码

**禁止使用 `-O2` 或更高优化级别调试**，因为：
- 代码可能被内联、重排
- 变量可能被优化掉或放入寄存器
- 调试信息不准确

```bash
# 正确做法
g++ -g -O0 main.cpp -o main

# 错误做法（难以调试）
g++ -g -O2 main.cpp -o main
```

### 调试符号剥离

如果程序没有调试信息：

```bash
# 检查调试符号
file main
readelf -S main | grep debug

# 如果没有，添加调试符号重新编译
g++ -g main.cpp -o main
```

## Windows 平台

### MinGW + GDB

```bash
# 安装 MinGW（包含 GDB）
# 从 https://www.mingw-w64.org/ 下载

# 编译
g++ -g -O0 main.cpp -o main.exe

# 调试
gdb ./main.exe
```

### VS Code 调试（推荐）

创建 `.vscode/launch.json`:

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "GDB 调试",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/main.exe",
            "args": [],
            "stopAtEntry": true,
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "miDebuggerPath": "C:/mingw64/bin/gdb.exe"
        }
    ]
}
```

### 使用 cgdb（ curses GDB）

提供源码高亮，比纯终端 GDB 更友好：

```bash
# 安装
sudo apt install cgdb

# 使用
cgdb ./main
```

## 常见调试场景

### 1. 段错误（Segmentation Fault）

```bash
gdb ./main
(gdb) run
# 崩溃后
(gdb) bt              # 查看调用栈，找到崩溃位置
(gdb) frame 0         # 切换到崩溃的栈帧
(gdb) info locals     # 查看局部变量
```

### 2. 内存泄漏

配合 Valgrind：

```bash
valgrind --leak-check=full ./main
```

### 3. 死锁（多线程）

```bash
gdb ./main
(gdb) break deadlock_function
(gdb) run
(gdb) info threads
(gdb) thread apply all bt
```

### 4. 数组越界

```bash
(gdb) break access_array_out_of_bounds
(gdb) run
(gdb) watch global_array
(gdb) continue    # 越界写入时停下
```

## 练习建议

1. 编译 `main_bug` 版本，用 GDB 单步跟踪 `increment_counter_bug`
2. 设置条件断点定位 `global_array[10]` 越界
3. 对 `factorial_bug(-1)` 设置监视点观察无限递归
4. 在多线程版本中用 `watch` 定位竞态条件

## 参考链接

- [GDB 官方文档](https://sourceware.org/gdb/current/onlinedocs/gdb/)
- [GDB 多线程调试](https://sourceware.org/gdb/current/onlinedocs/gdb/Threads.html)
