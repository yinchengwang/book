# error_handling scaffold

C 错误处理演示——`errno`/`perror`/`strerror`/`assert` + 自定义错误码 + 防御式编程。

## 复现命令

```bash
cd learning/scaffold/error_handling

gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
```

## 预期输出（节选）

```
[errno] === errno 全局错误码 ===
  fopen 失败, errno = 2 (ENOENT=2)
  strtol 溢出, errno = 34 (ERANGE=34), 返回 9223372036854775807

[perror] === perror 自动打印 errno 描述 ===
  fopen failed: No such file or directory

[strerror] === strerror(errno) 转字符串 ===
  EACCES = 13 -> "Permission denied"
  EBADF = 9 -> "Bad file descriptor"
  ENOENT = 2 -> "No such file or directory"
  ENOMEM = 12 -> "Not enough space"
  ERANGE = 34 -> "Numerical result out of range"

[assert] === assert 调试断言 ===
  NDEBUG 未定义，assert 生效
  assert(x > 0) 通过 (x=42)

[defensive] === 防御式编程（错误码返回） ===
  safe_divide(10, 3) = 3, rc=0 (OK)
  safe_divide(10, 0) = rc=-1 (ERR_INVALID_ARG)
  safe_divide(10, 3, NULL) = rc=-3 (ERR_NULL_POINTER)
  safe_divide(INT_MIN, -1) = rc=-4 (ERR_OVERFLOW, 防 UB)

=== PASS ===
```

## 关键点

- **`errno` 是 `<errno.h>` 定义的 `int` 全局变量**：库函数失败时设置，**调用前必须 `errno = 0` 重置**（避免残留值）
- **`perror(msg)` 自动拼 errno**：格式 `"msg: errno 描述\n"`，比 `printf + strerror` 简单
- **`strerror(errno)` 返回字符串**：可自定义前缀；**线程安全用 `strerror_r`**
- **`assert(expr)` 调试断言**：`NDEBUG` 定义时被 `#define` 为 `((void)0)` 禁用——生产 release 编译加 `-DNDEBUG`
- **防御式编程**：检查输入参数（NULL/范围/溢出），**返回错误码而非崩溃**——C 没有异常机制
- **整数溢出 UB**：`INT_MIN / -1` 是 UB（结果溢出），必须先检查

详见 NOTES.md 工程对照段。