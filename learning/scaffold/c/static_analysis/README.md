# static_analysis scaffold

静态分析工具实践——代码故意包含 6 种可被 GCC/clang-tidy/cppcheck 检测的问题。不依赖运行时，所有问题在编译期可见。

## 复现命令

```bash
cd learning/scaffold/static_analysis

# 1. 编译运行（warning 仍会产生可执行文件）
gcc -Wall -Wextra -Wpedantic -Wconversion -std=c11 -o static_analysis_demo main.c
./static_analysis_demo

# 2. 仅编译检查（不链接），观察 warning
gcc -Wall -Wextra -Wpedantic -Wconversion -std=c11 -c main.c -o /dev/null

# 3. 把 warning 当 error（CI gate 模式）
gcc -Wall -Wextra -Werror -std=c11 -c main.c -o /dev/null
# ↑ 这条命令会失败（warning→error）——这正是 CI 中想要的效果

# 4. cppcheck（如已安装）
cppcheck --enable=all main.c
```

## 静态分析工具对比

| 工具 | 平台 | 检测维度 | 本机可用 | 特点 |
|------|------|---------|---------|------|
| GCC -Wall -Wextra | 全平台 | 语法/类型/未定义行为 | ✅ | 零配置，编译即检查 |
| cppcheck | 全平台 | 逻辑错误/资源泄露/UB | ❌ (需安装) | 误报率极低 |
| clang-tidy | 全平台 | 风格/现代C++/性能 | ❌ (需安装) | 可定制规则库 |
| flawfinder | 全平台 | 安全漏洞 | ❌ (需安装) | CWE 映射 |
| clang-format | 全平台 | 格式/风格 | ❌ (需安装) | 只检查不分析 |
| ASAN/UBSAN | gcc/clang | 内存/未定义行为 | ❌ (MinGW) | 运行时检测 |

## 关键点

- **编译器的 warning 是最廉价的静态分析**：开 `-Wall -Wextra` 能拦下 ~60% 的 trivial bug
- **`-Werror` 是 CI gate 的标准做法**：warning 不在 CI 中过——任何 warning 阻止 merge
- **cppcheck 零误报**：它的设计哲学是"宁可漏报，不误报"——在 Linux 内核等超大型 C 项目中广泛使用
- **抑制误报**：`/* fallthrough */` 注释告诉编译器"我是故意的"；`_Pragma("GCC diagnostic ignored \"-Wunused\"")` 局部关闭某条 warning

详见 NOTES.md 工程对照段。
