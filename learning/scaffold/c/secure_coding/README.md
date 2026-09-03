# secure_coding scaffold

五类 C 语言经典安全漏洞 + 安全实践对照。每类漏洞配 safe 实现。默认编译 safe 版本，`-DVULN_DEMO` 观察漏洞行为。

## 复现命令

```bash
cd learning/scaffold/secure_coding

# Safe 版本（默认）
gcc -Wall -Wextra -Wformat=2 -Wformat-security \
    -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
    -O2 -std=c11 -o secure_demo main.c
./secure_demo

# 漏洞版本（观察 unsafe 行为）
gcc -DVULN_DEMO -o secure_demo_vuln main.c
./secure_demo_vuln
```

## 五类漏洞 vs CWE

| # | 漏洞类型 | CWE | 检测方式 | 安全实践 |
|---|---------|-----|---------|---------|
| 1 | Buffer Overflow | CWE-120 | ASAN / Valgrind / `-fstack-protector` | `strncpy` + null 终止 |
| 2 | Format String | CWE-134 | `-Wformat=2 -Wformat-security` | `printf("%s", input)` |
| 3 | Integer Overflow | CWE-190 | `__builtin_mul_overflow` / UBSAN | 分配前检查溢出 |
| 4 | Use-After-Free | CWE-416 | ASAN / Valgrind | `free(p); p = NULL` |
| 5 | TOCTOU | CWE-367 | 人工审计（静态分析难检测） | fd-based 操作（fstat 验证 inode） |

## 关键点

- **缓冲区溢出是最致命的漏洞**：栈溢出可覆盖返回地址实现任意代码执行。`strncpy` + 强制 null 终止是最小防御
- **格式字符串漏洞可读/写任意内存**：`printf(user_input)` 中如果包含 `%n`（写入已打印字符数到地址），可修改任意内存位置
- **整数溢出导致堆分配不足**：`malloc(count * size)` 如果 count=2³⁰、size=2³⁰，乘积截断为小值——实际只分配少量内存，后续写入触发堆溢出
- **Use-After-Free 是最难调试的 bug**：free 后指针仍可读写（那块内存可能已被其他分配复用）——行为取决于内存分配器的状态，不是每次运行都能复现
- **TOCTOU 是竞态条件的安全版**：文件系统不是你独占的——检查文件是否存在和实际使用文件之间，另一个进程可能替换/删除了它

详见 NOTES.md 工程对照段。
