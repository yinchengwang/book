# valgrind scaffold

3 个故意内存问题 + valgrind 截获示范。MinGW 不提供 valgrind，但 **GCC `-Wall -Wextra` 编译期已捕捉 2 个**；其余需 Linux 用户跑 valgrind。

## 复现

```bash
cd learning/scaffold/valgrind

# MinGW 用户（无 valgrind）
make
./valgrind_demo   # leak, uaf warning, uninit sum 随机
make run          #

# Linux 用户
make valgrind     # valgrind --leak-check=full 打印详细报告
```

## 预期输出（MinGW）

```
[valgrind] uaf str = '...'  (读取 free 后的垃圾)
[valgrind] uninit sum = 随机 (栈残留)
this branch may run depending on stack dirt
```

Favor in `-Wuse-after-free` warning + 内存泄漏（GCC 不拦截）。

## 关键点

- **valgrind 的 `leak-check=full`** 报告 "definitely lost" 等分类——与 ASAN 侧报互异
- **`--track-origins=yes`** 告诉你是哪里分配的那块未初始化内存
- **`-g` 调试符号必须**：valgrind 可报告具体源码行号
- **`-O0` 推荐**：-O2 把变量优化掉，valgrind 报告失真
- **MinGW 无 valgrind → 用 `-Wall -Wextra` 编译期取替代**

详见 NOTES.md 工程对照段。
