# gdb 学习笔记

## 概念地图

`gdb`（GNU Debugger）是 POSIX 平台事实标准调试器。它的核心能力：

- **断点**：停在某行 / 某函数 / 某条件
- **栈追溯**（backtrace, bt）：看 crash 时调用链
- **变量观察**（print, info locals/watch）：检查运行时状态
- **步进**（next/step/finish）：逐行 / 进入函数 / 跳出函数
- **多线程**（thread N，info threads）：每个线程独立栈
- **远程调试**（gdbserver）：target / host 模式，嵌入式与生产环境调试

最小命令集：
```
(gdb) run                        # 启动进程
(gdb) break main.c:50            # 在 50 行打断点
(gdb) continue                   # 跑到断点
(gdb) print *(int*)p             # p 作为 int * 解引用
(gdb) next                       # 下一行（不进入函数）
(gdb) step                       # 进入函数
(gdb) finish                     # 跑到当前函数末尾
(gdb) bt                         # backtrace
(gdb) frame N                    # 切换栈帧
(gdb) info locals                # 本帧的所有局部
(gdb) watch x                    # x 变化时停
```

调试 symbol 质量：`-g` 必须 + `-O0` 推荐；`-O2` 会优化掉中间变量、inline 函数、循环展开，让 gdb 视图与源码 mismatch。

编译期后台：ASAN（AddressSanitizer）能在 runtime 直接捕获 buffer overflow / use-after-free / NULL deref，比纯 gdb 友好。Linux GCC/Clang 都默认带；本机 mingw 没 libasan 库需要走纯 gdb 路径。

## 踩坑记录

1. **本机 MinGW 缺 ASAN 库**：`gcc -fsanitize=address` 报 `cannot find -lasan`。只能走纯 gdb 路径，**这是 R5 中第一张（也是唯一一张）本机 MinGW 实测 PASS 的卡**——pthread 也 PASS，但语义完全不同。
2. **`-O2` 下 gdb 错位**：用 `-O2` 优化后，源码第 28 行可能根本不存在（被 constexpr 化）。本 demo Makefile 默认 `-O0`。
3. **Bug A 隐患**：用了 `sizeof(VOWELS)` 在文件作用域 `char VOWELS[VOWELS_SIZE]` 上是 10 字节——但读者很容易误以为是数组长度本身。改成 `VOWELS_SIZE` 宏更明示。
4. **Bug B 数组越界**：本 demo 因 `if (idx >= 0 && idx < VOWELS_SIZE)` 没真越界，但属于**潜在 bug**——读者加一个 assertion 或 assert-style 早期失败可提前发现。
5. **Bug C 空指针 strlen**：实测本机 `./buggy`（无参数）`Segmentation fault`，bash 把信号翻译给用户看到了。本机 Linux 上 gdb 会打印 trap。
6. **gdb 的 batch 模式**：`gdb -batch -ex '...' -ex '...'` 让 gdb 不交互直接退出，适合 CI 采集 crash 信息；本 README 给出示例。

## 工程对照（≥100 字硬约束）

gdb 在工程侧实战有以下直接迁移价值：

1. **`engineering/src/db/storage/bufmgr.c` 段错误调试**：BufMgr 在 eviction 时若 LRU 链环未更新好会 crash；本卡刷过后读者能用 gdb 进 Clock-Sweep 找到 `unpin_page` 后的指针稳定问题。
2. **`engineering/src/db/consensus/raft.c` election 卡死**：debug 时常用 `gdb --args ./raft_test --election-timeout 50 --thread N` 多线程调试。本卡提供 thread 切换与 watch 命令基础。
3. **`engineering/src/db/index/vector_index/hnsw/*.c`**：HNSW 图遍历遇到 cycle 时入栈溢出；用 gdb `bt 30` 看 30 层栈，配合代码 `graph_traverse` 找出 cycle 的入口卡 ID。
4. **`rag/include/rag/engine.h` 远程索引 crash**：未来 RAG Engine 调 Engineering REST Server 返回 HTTP 200 但 body 错位时——`watch` 协议层 buffer + `condition` 断点能定位反序列化 bug。
5. **PG 风格 postmaster.c 子进程调试**：daemon 后 fork 的子进程不可达，gdbserver attach PID 是工程标准手法——本卡 `run` + attach 是入门。

**实战配方**：
```bash
# 给一段工程侧 panic 时的复现脚本：
gdb -batch \
    -ex 'set pagination off' \
    -ex 'break main' \
    -ex 'run --repro-case=fail-2026-07-11' \
    -ex 'bt 50' \
    -ex 'info locals' \
    -ex 'info threads' \
    -ex 'thread apply all bt 5' \
    ./test_binary 2>&1 | tee crash-$(date +%s).log
```

这是 R5 中"工程侧实战 trick"的具体形态，与 §10 验证项"抽查"对应。
