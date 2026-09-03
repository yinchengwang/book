# HugePages 大页机制

## 功能概述

演示 Linux HugePages 大页内存分配机制，提升 TLB 命中率。

## 核心内容

- hugetlbfs 文件系统挂载
- mmap + MAP_HUGETLB 映射大页
- TLB 优化原理（4KB vs 2MB vs 1GB）
- 数据库 HugePages 配置

## 编译运行

```bash
# 配置 HugePages（需要 root）
sudo sysctl -w vm.nr_hugepages=20

# 编译运行
gcc -std=c11 -Wall -o hugepages main.c
sudo ./hugepages
```

## 依赖

- Linux 系统
- root 权限（MAP_HUGETLB）
- 已配置 HugePages（nr_hugepages > 0）

## 输出示例

```
--- Demo 1: 检查 HugePages 配置 ---
[hugetlb]   HugePages_Total:     20
[hugetlb]   HugePages_Free:      20
[hugetlb]   Hugepagesize:     2048 kB

=== PASS ===
```
