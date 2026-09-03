# valgrind 学习笔记

## 概念地图

Valgrind 是 Linux/UNIX 运行时内存分析器，通过动态二进制插桩（DBI）拦截 99% 的内存错误：

- **Leak 检查**：definitely lost（确定泄漏）、indirectly lost（间接泄漏）、possibly lost（疑似泄漏）
- **Use-after-free**：free 后读写
- **Uninitialized**：读未初始化栈/堆
- **Overlap**：memcpy 等地址重叠（valgrind 的 `--check-overlap=yes`）
- **L1/L2 cache profiling**：cachegrind 子工具
- **Heap profiling**：massive 子工具

Valgrind 与 -fsanitize 系的互补：

| 工具 | 性能开销 | 覆盖率 | 部署方式 |
|---|---|---|---|
| Valgrind | 3-30x 慢 | 内存 99% | `valgrind ./binary` |
| ASAN | 2x 慢 | 内存 90% | 编译期 -fsanitize=address |
| UBSAN | 低 | UB 90% | 编译期 -fsanitize=undefined |
| TSAN | 5x | 竞态 99% | 编译期 -fsanitize=thread |

## 踩坑记录

1. **本机 MinGW 无 valgrind**：只能用 GCC -Wxxx warning 系列做编译期等价检测。`-Wuse-after-free` 在编译期就抓到 uaf——比运行期 valgrind 更友好。
2. **valgrind 的 `definitely lost` vs `possibly lost`**：前者指绝对没指针指向（泄漏）；后者指内部还有指针但 root set 遗失（可能不算泄漏）。
3. **valgrind 与 ASAN 地址冲突**：同时跑会 double-tool 报错；必须只转一个。工程侧应选 ASAN 用于 CI（更快），valgrind 用于深度审计。
4. **uninit 值的"残留"含义**：无任何初始化（栈上置零到 `-fno-zero-initialized-in-bss` 关闭）；读后看到的是前一个函数或页留下来的垃圾值——这是 0-mark vulnerability 的来源之一。
5. **本 demo 的 uninit 每跑一次数值不同**：这是 valgrind 要抓的——"this branch may run depending on stack dirt"，潜在 UB。

## 工程对照（≥100 字硬约束）

Valgrind 在工程侧有以下应用：

1. **`engineering/tests/` 目录**：valgrind 测试套件 `test_bufmgr_valgrind` 已经存在（见 `engineering/test/db/storage/bufmgr/`），跑 `valgrind --leak-check=full ./test_binary` 输出现有测试的堆完整性报告。
2. **`engineering/src/db/storage/bufmgr.c` 的 eviction 检查**：Clock-Sweep 抖动时若漏 unpin 会累积泄漏——valgrind 能抓到"整个测试跑完后还有 Buffer 节点未释放"。
3. **`rag/src/rag/index/` 现有的 shared_ptr 泄漏**：R5 OPSX `rag-remote-index-backend` 接入了 IBm25Index 抽象；shared_ptr 循环引用是标准 valgrind 能截获的 "indirectly lost"。
4. **`engineering/src/db/api/handlers.c` 的 HandlerContext**：大请求内存临时分配后未 free 是 leak；valgrind 会在 shutdown 时报告 "X bytes still reachable"。

对 CI 的常规集成（与 §8 code_style + §6 undefined_behavior 联手）：

```bash
# CI 记忆配方（可放入 .github/workflows/ 或 make valgrind-ci）
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --error-exitcode=1 --log-file=valgrind-report.log \
    ./test_binary
```

学完本卡能动手的事：

- 给 `engineering/scripts/` 加 `valgrind-ci.sh` 模板，后续 OPSX 可引用它做 CI gate。
- 跑一次 `valgrind --leak-check=full ./test_bufmgr` 看现有的 buffer 使用情况，记录到 NOTES。
