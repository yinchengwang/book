# stdlib 学习笔记

## 概念地图

`<stdlib.h>` 是 C 标准库的"实用工具箱"——内存管理、进程控制、字符串转换、排序、随机数：

- **内存管理四件套**：
  - `malloc(size)` —— 分配 size 字节，**不初始化**
  - `calloc(n, size)` —— 分配 n×size 字节，**自动清零**
  - `realloc(p, size)` —— 调整 p 指向的内存大小，**可能搬移**
  - `free(p)` —— 释放，**p=NULL 是 no-op**
- **进程控制三件套**：
  - `exit(status)` —— 触发 atexit 回调链 + 刷新 stdio + `_exit(status)`
  - `atexit(fn)` —— 注册清理回调（LIFO 顺序）
  - `abort()` —— 异常终止，**不触发 atexit**，发 SIGABRT
- **字符串转整数三件套**：
  - `atoi(str)` —— 简单但不检错，转换失败返回 0
  - `strtol(str, &endptr, base)` —— 安全版本，支持 2/8/10/16 进制
  - `strtod`/`strtof` —— 浮点版本
- **排序与查找**：
  - `qsort(base, n, size, cmp)` —— 通用排序，平均 O(n log n)
  - `bsearch(key, base, n, size, cmp)` —— 二分查找，**要求输入已排序**
- **环境与系统**：
  - `getenv("PATH")` —— 读环境变量
  - `system("ls")` —— 调 shell 命令执行
  - `getenv` / `setenv` —— 进程环境
- **随机数**：
  - `rand()` —— [0, RAND_MAX] 伪随机，**确定性**（同种子同序列）
  - `srand(seed)` —— 设种子，`time(NULL)` 作为种子常见

## 踩坑记录

1. **`realloc` 失败返回 NULL**：原指针仍有效——**必须 `tmp = realloc(p, ...); if (!tmp) free(p);`**
2. **`malloc(0)` 行为**：C 标准允许返回 NULL 或有效指针。**永远 `malloc(n*sizeof(T))` 而非 `malloc(n)`**
3. **`free` 后使用指针**：UAF（use-after-free），是头号安全漏洞
4. **`exit` vs `_exit`**：`exit` 走 atexit + 刷新 stdio；`_exit` 直接系统调用。**fork 后子进程用 `_exit`**
5. **`rand() % n` 不均匀**：低位数周期短。**用 `(rand() / (RAND_MAX+1.0)) * n`**
6. **`qsort` 比较函数返回 int**：溢出时 `a - b` 可能溢出。**用 `(a > b) - (a < b)`**

## 工程对照（≥100 字硬约束）

`<stdlib.h>` 在本仓库 `engineering/` 中体现为基础设施：

1. **`qsort` 在 `engineering/src/algo-prod/sort/sort.c`**：所有排序算法都用 `qsort` 作为"对照组"——例如实现快速排序时与 `qsort` 比较耗时/正确性。`qsort` 是 glibc 实现的 introsort（quicksort + heapsort + insertion sort 混合），O(n log n) 最坏保证
2. **`getenv` 在 `engineering/src/db/core/initdb.c`**：`initdb` 读取 `PGDATA`、`PGUSER`、`PGPORT` 等环境变量作为默认值。`getenv` 不存在返回 NULL，**必须 NULL 检查**
3. **`system()` 在 `learning/scaffold/process/main.c`**：R9 卡中调用 `system("ps -o pid,ppid,stat,cmd ...")` 查询子进程状态。`system` 在 `fork + exec + waitpid` 之上封装，**比 fork+exec 简单但慢且阻塞**
4. **`exit/atexit` 在 `engineering/src/db/core/db_server.c`**：数据库服务器优雅关闭时调用 `atexit` 注册清理函数——关闭 fd、刷 WAL、记录 shutdown 日志。LIFO 顺序保证"后注册先清理"（例如先关网络监听，再刷 WAL）
5. **`malloc` 在 `engineering/src/db/storage/page/page.c`**：所有页面分配走 `malloc(page_size)`——`page_size` 通常 8KB。**生产环境可能改用 jemalloc/tcmalloc 替代**
6. **`calloc` 在 `engineering/src/db/storage/vector/vector_segment.c`**：向量 segment 分配走 `calloc`——因为 segment 头必须清零，未清零的 page 会导致脏读
7. **`strtol` 在 `engineering/src/db/cli/cli_main.c`**：CLI 解析端口号、buffer 大小等数字参数用 `strtol` 而非 `atoi`——**检测非法输入 + 报告行号**

学完本卡能动手的事：扫描 `learning/scaffold/` 下所有 main.c，把 `atoi` 改为 `strtol`，加上 `errno == ERANGE` 错误检查。