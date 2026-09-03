# secure_coding 学习笔记

## 概念地图

C 语言的"不安全"不是缺陷——它是设计哲学的结果：信任程序员，不设运行时检查。安全编程就是在不改变这种信任的前提下，**显式地**加入防御层：

- **缓冲区溢出（CWE-120）**：数组不做越界检查——`char buf[16]; strcpy(buf, long_input)` 静默覆盖栈上的返回地址。防御层次：1) `strncpy(dst, src, n-1)` + `dst[n-1]='\0'`（最小防御），2) `-fstack-protector-strong`（编译器插桩 canary），3) ASAN（运行时 shadow memory 检测）。本卡用第 1 层
- **格式字符串（CWE-134）**：`printf` 是可变参数函数——如果调用方不传参数但格式串中含 `%x`，`printf` 会从栈上读"不存在"的参数。`%n` 甚至能把已打印字符数写入栈上的任意地址。防御：**永远用 `printf("%s", input)` 而非 `printf(input)`**——一行代码的差异就是漏洞与安全的边界
- **整数溢出（CWE-190）**：C 的整数溢出是"静默回绕"（wraparound）——`INT_MAX + 1 == INT_MIN`。malloc 的参数是 `size_t`——`count * item_size` 可能溢出为一个小数，导致实际分配的内存远小于预期。防御：`__builtin_mul_overflow()`（gcc/clang 内建）或手动 `if (count > SIZE_MAX / item_size) return NULL`
- **Use-After-Free（CWE-416）**：free 释放了内存，但指针仍然持有那个地址——如果内存分配器把同一块内存给了另一个变量，通过悬空指针的读写就污染了那个变量。防御：**free 后立即 `p = NULL`**——下次解引用时触发确定性的段错误，而非不确定性的数据损坏
- **TOCTOU（CWE-367）**：`if (access(path, R_OK) == 0) { fd = open(path, O_RDONLY); }`——access 和 open 之间，另一个进程可能替换了文件（symlink attack）。防御：直接用 `open()` + `fstat()` 验证 fd 指向的 inode，而非用 path 二次检查

## 踩坑记录

1. **`strncpy` 不是 `strcpy` 的安全替代**：`strncpy` 在 src 长于 n 时不加 '\0'——如果不用 `dst[n-1]='\0'` 强制终止，后续对 dst 的读取同样会越界。`strlcpy`（BSD 扩展）解决了这个问题但在 glibc 中未包含
2. **`-D_FORTIFY_SOURCE=2` 需要优化开启**：这个宏在 `-O0` 下不生效——它依赖编译器的常量传播和死代码消除来判断"溢出是否确定发生"。永远在 `-O2` 下搭配使用
3. **`__builtin_mul_overflow` 的可移植性**：它是 gcc 5+ 和 clang 3.8+ 的内建函数——MSVC 用 `_Check_mul_overflow`。跨平台代码需要 `#ifdef` 封装
4. **Use-After-Free 的隐蔽性**：free 后立即再分配同样大小的内存——`malloc(32)` 大概率返回刚 free 的同一地址，此时悬空指针的读写看起来"正常"但已污染了新的合法数据。这是 use-after-free 最危险的地方——它可以潜伏多年不触发，直到内存分配器行为改变
5. **本机 MinGW 无 ASAN**：`-fsanitize=address` 在 MinGW 下链接阶段失败（缺少 `libasan`）。用 `-fstack-protector-strong` + `-Wformat-security` 做编译期替代

## 工程对照（≥100 字硬约束）

安全编程在本仓库的 C 代码中有直接对应的工程实现：

1. **Redis SDS — 安全字符串的范本**：`engineering/src/redis/redis_sds.c` 中的 `sdscatlen()` 每次追加都检查容量并自动扩容——`if (newlen > sdsavail(s)) sdsMakeRoomFor(s, newlen-curlen)`。这与本卡 `strncpy` + null 终止的设计同源，但 SDS 更进一步：它在 header 中存储了 `len` 和 `alloc`，做到了"绝对不会越界"——代价是每次字符串操作都多一次边界检查
2. **Page 管理的溢出风险**：`engineering/src/db/storage/page/page.c` 中的页面管理涉及大量 `page_size * num_pages` 的乘法——如果 page_size=8192 且 num_pages 由用户配置，整数溢出可能导致实际分配的 buffer 远小于预期。本卡的 `safe_int_overflow` 检查模式可直接应用——在 `page_init()` 中加入 `__builtin_mul_overflow` 检查
3. **handlers.c 的输入验证缺失**：`engineering/src/db/api/handlers.c` 的 9 个 REST handler 从 HTTP query string 中解析参数——用户输入的 key/value 长度不可信。本卡的缓冲区溢出和格式字符串漏洞直接对应：handler 的 `sscanf(url, "key=%s", buf)` 是无边界检查的（应该用 `%31s` 限制最大字段宽度）
4. **magic number + poison value 模式**：`engineering/src/db/storage/bufpage.h` 中页面头部的 `pd_checksum` 是数据损坏检测（类似 canary 检测栈溢出）——如果页面被意外修改（内存踩踏、磁盘 bit flip），checksum 不匹配就能在"数据被使用前"捕获问题。这与 `-fstack-protector` 的 canary 原理同构
5. **错误码体系 = 安全防线**：`engineering/src/db/core/errors.c` 的 `SYS_ERROR_*` 和 `DB_ERROR_*` 错误码迫使每个函数的调用者**检查返回值**——这是 TOCTOU 防御的工程化：不信任任何操作的返回值，每次调用后立即验证

学完本卡后能动手的事：审计 `engineering/src/db/api/handlers.c` 的 9 个 handler 函数——找出所有未做长度限制的 `sscanf`/`sprintf`/`strcpy` 调用，用本卡的 `strncpy` + null 终止模式替换，用 `%.31s` 格式限制 `sscanf` 的字段宽度。
