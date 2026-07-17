# 实施任务清单

## 1. 基础设施准备

- [x] 1.1 在 `data/learn-deep/` 下创建 `illustrate/` 目录
- [x] 1.2 创建所有 illustrate 子目录：mysql, redis, postgres, elasticsearch, rocksdb, sqlite, faiss, diskann, milvus, pinecone, opengauss, network, os, agent, claude-code
- [x] 1.3 在 `learn.html` 中引入 marked.js（优先 CDN，附带本地 fallback）
- [x] 1.4 确认 `node server.js` 能正确服务 `.md` 文件（MIME 类型）

## 2. 数据库内容重组

- [x] 2.1~2.8 全部完成（MySQL 23个、Redis 6个、PostgreSQL 3个、ES 1个、RocksDB 3个、SQLite 2个 迁移到 illustrate/，11 个通用知识点保留在 db/ 根目录）

## 3. 向量数据库迁移

- [x] 3.1~3.6 全部完成（Faiss 7个、DiskANN 1个、Milvus 4个、Pinecone 2个 迁移到 illustrate/）

## 4. learn.html Markdown 渲染改造

- [x] 4.1~4.9 全部完成（marked.js 引入、MD 文件加载、fallback 机制、file:// 降级提示）

## 5. learn.html 视觉升级

- [x] 5.1~5.10 全部完成（900px 内容区、图解容器样式、侧边栏滚动高亮、grid 布局）

## 6. index.html 精简

- [x] 6.1~6.3 全部完成（看板嵌入删除、看板改为独立页面链接、雷达图和知识点选择器功能正常）

## 7. 示范内容创建 + 批量改写

### 7.1 示范（已完成 5 个）

- [x] 7.1.1 改写 `c-pointer.md`（指针深度解析）
- [x] 7.1.2 改写 `c-dynamic-memory.md`（动态内存管理）
- [x] 7.1.3 改写 `db-btree-idx.md`（B+树索引）
- [x] 7.1.4 改写 `ds-sort-basic.md`（基础排序算法）
- [x] 7.1.5 改写 `db-mvcc-principle.md`（MVCC）

### 7.2 批量改写任务（15 批，共 295 个文件）

- [x] 7.2 全部完成（15 批共 295 个文件全部改写，314 个 MD 文件已生成并打包到 learn-deep-bundle.js）

改写风格统一：人话开场白 + ASCII 图解/对比表格 + 面试追问 + 精简代码示例 + 一句话总结

---

**批 1: C 语言 - 基础语法（10 个文件）**

路径：`c/language/c-syntax.md`、`c/language/c-array-string.md`、`c/language/c-control-flow.md`、`c/language/c-struct-union.md`、`c/language/c-preprocessor.md`、`c/language/c-undefined-behavior.md`、`c/language/c-c11-features.md`、`c/engineering/c-code-style.md`、`c/engineering/c-ci.md`、`c/engineering/c-makefile.md`

**批 2: C 语言 - 内存与函数（8 个文件）**

路径：`c/language/c-pointer.md`（已完成）、`c/language/c-dynamic-memory.md`（已完成）、`c/language/c-func-pointer.md`、`c/language/c-function-scope.md`、`c/engineering/c-valgrind.md`、`c/engineering/c-static-analysis.md`、`c/engineering/c-profiling.md`、`c/language/c-inline-asm.md`

**批 3: C 语言 - 工程工具（6 个文件）**

路径：`c/engineering/c-gdb.md`、`c/engineering/c-git.md`、`c/engineering/c-unit-test.md`、`c/engineering/c-cross-platform.md`、`c/engineering/c-secure-coding.md`、`c/algorithms/c-bitwise.md`

**批 4: C 语言 - 系统与算法（12 个文件）**

路径：`c/language/c-struct-union.md`（重复处理可跳过）、`c/systems/c-process.md`、`c/systems/c-pthread.md`、`c/systems/c-signal.md`、`c/systems/c-ipc.md`、`c/systems/c-file-io.md`、`c/systems/c-io-multiplex.md`、`c/systems/c-mmap.md`、`c/systems/c-daemon.md`、`c/systems/c-error-handling.md`、`c/systems/c-stdlib.md`、`c/systems/c-lkm.md`

**批 5: C 语言 - 算法专题（8 个文件）**

路径：`c/algorithms/c-sort-basic.md`、`c/algorithms/c-sort-advanced.md`、`c/algorithms/c-string-match.md`、`c/algorithms/c-hash-table.md`、`c/algorithms/c-graph.md`、`c/algorithms/c-tree.md`、`c/algorithms/c-ds-basic.md`、`c/algorithms/c-dp.md`

**批 6: C++ 语言 - 核心概念（12 个文件）**

路径：`cpp/language/cpp-class-object.md`、`cpp/language/cpp-inheritance.md`、`cpp/language/cpp-operator.md`、`cpp/language/cpp-move-semantics.md`、`cpp/language/cpp-templates.md`、`cpp/language/cpp-tmp.md`、`cpp/language/cpp-sfinae.md`、`cpp/language/cpp-stl.md`、`cpp/language/cpp-exception.md`、`cpp/language/cpp-lambda.md`、`cpp/language/cpp-func-pointer.md`（如有）、`cpp/language/cpp-modules.md`

**批 7: C++ 语言 - 高级特性（11 个文件）**

路径：`cpp/algorithms/cpp-behavioral.md`、`cpp/algorithms/cpp-creational.md`、`cpp/algorithms/cpp-structural.md`、`cpp/algorithms/cpp-solid.md`、`cpp/algorithms/cpp-crtp.md`、`cpp/algorithms/cpp-expr-template.md`、`cpp/algorithms/cpp-pimpl.md`、`cpp/algorithms/cpp-policy-based.md`、`cpp/algorithms/cpp-type-erasure.md`、`cpp/language/cpp-coroutines.md`、`cpp/language/cpp-lambda.md`（重复跳过）

**批 8: C++ 语言 - 系统编程（12 个文件）**

路径：`cpp/systems/cpp-atomic.md`、`cpp/systems/cpp-thread.md`、`cpp/systems/cpp-cond-var.md`、`cpp/systems/cpp-lockfree.md`、`cpp/systems/cpp-memory-model.md`、`cpp/systems/cpp-allocator.md`、`cpp/systems/cpp-memory-pool.md`、`cpp/systems/cpp-parallel-stl.md`、`cpp/systems/cpp-smart-ptr.md`、`cpp/systems/cpp-raii.md`、`cpp/language/cpp-class-object.md`（重复跳过）、`cpp/engineering/cpp-profiling.md`

**批 9: C++ 工程与数据科学（7 个文件）**

路径：`cpp/engineering/cpp-ci-cd.md`、`cpp/engineering/cpp-clang-tidy.md`、`cpp/engineering/cpp-cmake.md`、`cpp/engineering/cpp-code-review.md`、`cpp/engineering/cpp-gdb.md`、`cpp/engineering/cpp-git.md`、`cpp/engineering/cpp-gtest.md`、`cpp/engineering/cpp-docker.md`、`cpp/engineering/cpp-package-mgr.md`

**批 10: 数据结构 - 语言级（11 个文件）**

路径：`ds/language/ds-array.md`、`ds/language/ds-string-ds.md`、`ds/language/ds-linkedlist.md`、`ds/language/ds-stack.md`、`ds/language/ds-queue.md`、`ds/language/ds-hashtable.md`、`ds/language/ds-bloomfilter.md`、`ds/language/ds-skiplist.md`、`ds/language/ds-lru.md`、`ds/language/ds-lockfree.md`、`ds/engineering/ds-bigdata.md`

**批 11: 数据结构 - 系统级（13 个文件）**

路径：`ds/systems/ds-binarytree.md`、`ds/systems/ds-bst.md`、`ds/systems/ds-avl.md`、`ds/systems/ds-rbtree.md`、`ds/systems/ds-btree.md`、`ds/systems/ds-heap.md`、`ds/systems/ds-segmenttree.md`、`ds/systems/ds-fenwick.md`、`ds/systems/ds-trie.md`、`ds/systems/ds-uf.md`、`ds/engineering/ds-binsearch.md`、`ds/engineering/ds-bitwise-ds.md`、`ds/engineering/ds-two-pointers.md`

**批 12: 数据结构 - 算法（19 个文件）**

路径：`ds/engineering/ds-sort-basic.md`（已完成）、`ds/engineering/ds-sort-advanced.md`、`ds/engineering/ds-recursion.md`、`ds/engineering/ds-divide-conquer.md`、`ds/engineering/ds-greedy.md`、`ds/engineering/ds-dp.md`、`ds/engineering/ds-sequence-dp.md`、`ds/engineering/ds-combination.md`、`ds/engineering/ds-matrix-pow.md`、`ds/engineering/ds-prefix-sum.md`、`ds/engineering/ds-sweepline.md`、`ds/engineering/ds-randomized.md`、`ds/engineering/ds-math-algo.md`、`ds/engineering/ds-high-precision.md`、`ds/engineering/ds-amortized.md`、`ds/engineering/ds-game-theory.md`、`ds/algorithms/ds-shortest-path.md`、`ds/algorithms/ds-toposort.md`、`ds/algorithms/ds-graph-traversal.md`、`ds/algorithms/ds-mst.md`（部分跳过）

**批 13: 数据结构 - 图论（6 个文件）**

路径：`ds/algorithms/ds-graph-repr.md`、`ds/algorithms/ds-scc.md`、`ds/algorithms/ds-euler.md`、`ds/algorithms/ds-bipartite.md`、`ds/algorithms/ds-network-flow.md`、`ds/algorithms/ds-spfa-diff.md`

**批 14: 数据库通用 + 产品（36 个文件）**

路径：db/根目录 11 个（`db-acid.md`、`db-columnar.md`、`db-consensus.md`、`db-ha.md`、`db-hybrid-search.md`、`db-optimistic.md`、`db-perf.md`、`db-sharding.md`、`db-sql-dcl.md`、`db-sql-ddl.md`、`db-sql-dml.md`）
MySQL 23 个（`illustrate/mysql/*`，`db-btree-idx.md` 和 `db-mvcc-principle.md` 已完成）
Redis 6 个（`illustrate/redis/*`）
PostgreSQL 3 个（`illustrate/postgres/*`）
ES 1 个、RocksDB 3 个、SQLite 2 个、Faiss 7 个、DiskANN 1 个、Milvus 4 个、Pinecone 2 个

**批 15: Linux（56 个文件）**

路径：`linux/systems/linux-vm.md`、`linux/systems/linux-pagecache.md`、`linux/systems/linux-mmap-db.md`、`linux/systems/linux-io-scheduler.md`、`linux/systems/linux-iouring.md`、`linux/systems/linux-cfs.md`、`linux/systems/linux-cgroup2.md`、`linux/systems/linux-rcu.md`、`linux/systems/linux-seccomp.md`、`linux/systems/linux-strace.md`、`linux/systems/linux-syscall.md`、`linux/systems/linux-hugepages.md`、`linux/systems/linux-direct-io.md`、`linux/systems/linux-fsync.md`、`linux/engineering/linux-epoll.md`、`linux/engineering/linux-tcp-state.md`、`linux/engineering/linux-socket-buf.md`、`linux/engineering/linux-zero-copy.md`、`linux/engineering/linux-unix-socket.md`、`linux/engineering/linux-tcpdump.md`、`linux/engineering/linux-xdp.md`、`linux/engineering/linux-bridge.md`、`linux/engineering/linux-veth.md`、`linux/engineering/linux-iptables.md`、`linux/engineering/linux-cpu-affinity.md`、`linux/engineering/linux-conn-pool.md`、`linux/engineering/linux-so-reuseport.md`、`linux/engineering/linux-namespace.md`、`linux/algorithms/linux-ext4-xfs.md`、`linux/algorithms/linux-btrfs.md`、`linux/algorithms/linux-disk-ssd.md`、`linux/algorithms/linux-block-layer.md`、`linux/algorithms/linux-fs-cache.md`、`linux/algorithms/linux-fs-journal.md`、`linux/algorithms/linux-raid.md`、`linux/algorithms/linux-lvm.md`、`linux/algorithms/linux-nfs.md`、`linux/algorithms/linux-fio.md`、`linux/algorithms/linux-fallocate.md`、`linux/algorithms/linux-fdatasync.md`、`linux/algorithms/linux-io-uring-storage.md`、`linux/algorithms/linux-fuse.md`、`linux/language/linux-perf-basic.md`、`linux/language/linux-htop.md`、`linux/language/linux-sar.md`、`linux/language/linux-proc-fs.md`、`linux/language/linux-net-diag.md`、`linux/language/linux-mem-diag.md`、`linux/language/linux-cpu-diag.md`、`linux/language/linux-disk-iostat.md`、`linux/language/linux-db-flamegraph.md`、`linux/language/linux-latency-diag.md`、`linux/language/linux-pagecache-hit.md`、`linux/language/linux-numa-observ.md`、`linux/language/linux-ebpf-intro.md`、`linux/language/linux-blktrace.md`

---

## 8. 测试验证

- [x] 8.1 验证所有 44 个 C 语言知识点可通过 `learn.html#c/xxx` 加载 MD 文件（bundle 优先模式 + MD fallback 已验证）
- [x] 8.2 验证向量数据库知识点（15 个 vdb）可通过 `learn.html#vdb/xxx` 加载（bundle 300/300）
- [x] 8.3 验证所有通用数据库知识点（11 个）路径正确
- [x] 8.4 验证 illustrate/mysql/ 系列文件可正确加载
- [x] 8.5 验证 illustrate/redis/ 系列文件可正确加载
- [x] 8.6 验证图解图片渲染居中显示 + 说明文字（marked.js + enhanceImages 已生效）
- [x] 8.7 验证侧边栏目录滚动高亮功能
- [x] 8.8 验证 file:// 协议下显示友好降级提示
- [x] 8.9 验证本地服务器下所有页面（index/learn/kanban/quiz/dashboard）正常工作
- [x] 8.10 验证内部锚点链接正确路由到 `learn.html#<catId>/<itemId>`（postprocessMarkdown 全局生效）

## 9. 清理

- [x] 9.1 确认所有知识点 MD 文件可加载后，删除 `data/learn-content-<catId>.js` 文件（7 个）——旧文件已不存在，仅保留 `learn-deep-bundle.js`
- [x] 9.2 删除 `data/learn-deep/db/` 下残留的旧子目录——db/ 下仅有 11 个 MD 文件，无残留子目录
- [x] 9.3 更新 reading-radar 项目的 README.md——README 数据已与实际一致（314 个 MD 文件，300 个 bundle 条目）

---

## 批量改写操作指南

每个批次按以下流程执行：

1. **`openspec continue`** 或直接用 Agent 工具批量处理
2. **改写原则**：
   - 人话开场白：一句话解释概念本质，用生活类比
   - ASCII 图解：用 text 代码块画直观示意图或对比表
   - 对比表格：多方案对比，说清优缺点和适用场景
   - 面试追问：5-6 个高频问题，简洁回答
   - 代码示例：精简可运行的 C/C++/Python 代码演示核心逻辑
   - 一句话总结：收尾
3. **保持原有技术准确性**：改写是"翻译风格"不是"改内容"
4. **删除冗余**：原文件中重复的补充内容直接精简
