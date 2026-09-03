# 堆栈溢出/尾递归

## 目录结构

```
stack_overflow/
├── main.c     # 堆栈溢出与尾递归演示代码
├── Makefile   # 构建和运行脚本
└── README.md  # 本文件
```

## 功能说明

演示 C 语言中的栈相关概念：

- **栈帧分析**: 递归调用时的栈布局
- **尾递归优化**: 编译器将尾递归转为循环（需 gcc -O2）
- **栈缓冲区溢出**: strcpy 不安全使用的后果
- **VLA (变长数组)**: 栈上分配大数组导致溢出
- **Canary 检测**: 手工模拟栈保护检测

## 如何运行

```bash
cd learning/scaffold/grokking/stack_overflow/
# 编译（默认 -O0, 无尾递归优化）
make
./main

# 或带尾递归优化的版本
gcc -std=c11 -Wall -O2 main.c -o main_opt
./main_opt
```
