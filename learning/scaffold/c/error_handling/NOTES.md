# error_handling 学习笔记

## 概念地图

C 语言的错误处理哲学是"**返回值 + 全局 errno**"——C 没有 try/catch，没有 exception，错误必须显式处理：

- **errno 三件套**：
  - `errno` —— `<errno.h>` 的全局 `int`，POSIX 库函数失败时设置
  - `perror(msg)` —— 打印 `"msg: errno 描述"`
  - `strerror(errno)` —— 把 errno 值转为可读字符串
- **assert 分级断言**：
  - `assert(expr)` —— 标准断言，`NDEBUG` 定义时被禁用
  - `static_assert(expr, msg)` —— C11 编译期断言（`#include <assert.h>`）
- **POSIX errno 常用值**：
  - `ENOENT (2)` —— 文件不存在
  - `EACCES (13)` —— 权限拒绝
  - `EBADF (9)` —— 坏文件描述符
  - `ENOMEM (12)` —— 内存不足
  - `ERANGE (34)` —— 数值超出范围
  - `EINVAL (22)` —— 非法参数
- **防御式编程三原则**：
  1. **检查输入**：函数入口校验所有参数（NULL/范围/边界）
  2. **返回错误码**：用 `enum` 定义错误码，而非 `int -1/-2`
  3. **不假设成功**：库函数返回值必检，`errno` 必读
- **错误传播链**：底层设 `errno` → 中层 `perror` → 上层包装成业务错误码 → 顶层日志/退出

## 踩坑记录

1. **`errno` 残留**：调用 `errno = 0` 前不要相信 errno 值。**库函数成功时 errno 不一定清零**
2. **`perror` 写到 stderr**：默认 stderr，**不写入日志文件除非重定向**
3. **`strerror` 线程不安全**：内部静态缓冲区，多线程竞争。**线程安全用 `strerror_r`**
4. **`assert` 副作用**：`assert(x++)` —— `NDEBUG` 下整条消失。**assert 内不放业务逻辑**
5. **`INT_MIN / -1` UB**：结果是 `INT_MAX + 1`（溢出）。**必须先检查**
6. **忽略返回值**：Linux 内核风格 `if (foo() < 0) handle_error();` ——C 项目代码量爆炸但安全
7. **`assert.h` vs `<assert.h>`**：C 标准 `<assert.h>`，**两种写法等价**

## 工程对照（≥100 字硬约束）

错误处理在本仓库 `engineering/` 中体现为"防御式编程"的标准范式：

1. **`engineering/src/db/core/errors.c` 错误码体系**：定义 PG 风格的错误码常量 + 错误消息表。所有 `engineering/src/db/**/*.c` 通过 `ereport(ERROR, errcode(ERRCODE_XXX), errmsg(...))` 报错。这是"枚举错误码 + 集中消息表"的工程范本
2. **`engineering/src/db/core/structured_log.c` 日志错误**：所有错误日志结构化——`log_error("page read failed: file=%s offset=%ld errno=%d", path, off, errno)` 三段式：操作 + 上下文 + errno 值
3. **`engineering/src/db/storage/page/disk.c` 错误返回**：所有磁盘 I/O 函数返回 `int`（0=OK，负数=错误码），不崩溃。**调用方必须检查返回值**
4. **`engineering/src/db/api/handlers.c` API 错误处理**：HTTP API 层把内部错误码映射为 HTTP 状态码——`ERR_INVALID_ARG → 400`、`ERR_NOT_FOUND → 404`。**两层错误码解耦**
5. **`engineering/src/db/storage/vector/vector_engine.c` 错误传播**：vector 引擎的每个操作返回 `VectorStatus` enum（`VECTOR_OK`/`VECTOR_ERR_NOMEM`/...）。**enum 而非 int 防止误用**
6. **`engineering/include/db/storage/page/page.h` errno 模式**：所有 page 操作函数声明"`返回 0 表示成功，-1 表示失败并设置 errno`"——模仿 POSIX 系统调用约定
7. **`assert` 在调试宏**：`grep -rn "assert(" engineering/src/algo-prod/` 显示算法代码大量 assert 不变量——例如 `assert(idx < n)` 防越界。release 编译 `-DNDEBUG` 全部消除

学完本卡能动手的事：扫描 `learning/scaffold/` 下所有 main.c，把 `assert(p != NULL)` 加到每个 `malloc` 调用前，把 `atoi` 改为 `strtol + errno 检查`。