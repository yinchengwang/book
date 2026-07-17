# 位运算技巧

## 简介

位操作在算法中的应用广泛，利用 CPU 直接对二进制位进行操作，可以在常数时间内完成许多看似复杂的任务。本卡片演示了位运算的核心技巧：

- **单独位操作**：设置（set）、清除（clear）、翻转（toggle）特定比特位
- **位移操作统计**：通过 Brian Kernighan 算法统计二进制中 1 的个数、判断 2 的幂
- **交换奇偶位**：利用位掩码同时交换相邻的奇偶位
- **反转字节序**：大端/小端之间的字节序转换

## 运行方法

```bash
# 编译
make

# 运行
make run

# 或直接编译运行
gcc -std=c11 -Wall -Wextra main.c -o bit_manipulation && ./bit_manipulation

# 清理
make clean
```
