# ci scaffold

持续集成与自动化——GitHub Actions CI 配置 + 本地 ci.sh 脚本 + YAML/GitHub Actions 速查。

## 文件结构

```
ci/
├── main.c                       # 极简被测程序（含 reverse_string 函数）
├── .github/workflows/ci.yml     # GitHub Actions 3-job 流水线
├── ci.sh                        # 本地 CI 脚本（编译+测试+静态分析+格式）
├── Makefile                     # 含 ci-local / check-yml / lint target
├── README.md                    # 本文件
├── NOTES.md                     # 学习笔记 + 工程对照
└── CHEATSHEET.md                # YAML + GitHub Actions 速查
```

## 复现命令

```bash
cd learning/scaffold/ci

# 编译 + 运行
gcc -Wall -Wextra -Wpedantic -O2 -std=c11 -o test main.c && ./test

# 本地 CI 流水线
bash ./ci.sh

# Makefile target
make ci-local        # 跑 ci.sh
make check-yml       # 校验 .github/workflows/ci.yml 语法
make lint            # cppcheck（不可用时 graceful 降级）
```

## 预期输出（main.c 运行）

```
[build] === 编译检查 ===
  gcc 编译通过

[lint] === 静态分析 ===
  gcc -Wall -Wextra -Wpedantic 零警告

[test] === 冒烟测试 ===
  reverse("hello") = "olleh"
  reverse("world") = "dlrow"
  reverse("racecar") = "racecar"
  reverse("") = ""
  3/3 tests passed

[fmt] === 格式统计 ===
  main.c: 90 行, 最大行长 80

=== PASS ===
```

## GitHub Actions 流水线（`.github/workflows/ci.yml`）

3 个 job：
1. **`build`** —— 多编译器矩阵（gcc 11/12/13/14 + clang 15）+ 缓存 + 上传 artifact
2. **`test`** —— 依赖 build，编译后运行测试
3. **`static-analysis`** —— cppcheck + flawfinder

触发：push 到 main/master/develop、PR、手动 workflow_dispatch。

## 关键点

- **CI 三件套**：编译（fail fast）+ 测试（功能正确）+ 静态分析（潜在缺陷）——**任何一件缺失都是 CI 漏洞**
- **矩阵策略**：用 `matrix.compiler × matrix.version` 自动测多编译器版本，**`fail-fast: false` 让单失败不阻塞其他组合**
- **`needs: build`** —— job 依赖关系图：test 依赖 build 先成功，避免重复编译
- **缓存 key**：`hashFiles('**/*.c', '**/*.h')` 让源码变更自动失效缓存
- **`workflow_dispatch`** —— 手动触发，方便调试
- **graceful 降级**：本地 Makefile `lint` target 用 `command -v X` 检测，工具缺失时 exit 0

详见 NOTES.md 工程对照段和 CHEATSHEET.md 速查表。