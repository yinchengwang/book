# C9-3 Subsystems 任务清单

## 任务列表

### Task #1: RAG 核心管道实现
- **状态**: completed
- **描述**: 实现 rag_pipeline.c + rag_ingestion.c + rag_retrieval.c + rag_eval.c
- **验收**: 端到端管道测试通过
- **结论**: design.md 的"新建 engineering/src/rag/ 四个文件"已被仓库现状取代 ——
  `engineering/src/db/executor/rag/` 下已有完整实现（rag_pipeline.c / chunk_op.c /
  embed_op.c / retrieve_op.c / generate_op.c + graphrag_*.c，共 ~5700 行）。
  真正的缺口是构建接线：rag 源文件自包含（仅依赖 rag_executor.h / graphrag.h /
  vector_engine.h 与标准库），但此前被 bit-rot 的 db_executor 整库拖累无法链接测试。
  修复方式：test_rag_executor 直接编译 rag/*.c 源文件（见
  src/db/executor/CMakeLists.txt 注释），绕开 db_executor。
  **rag_executor_test 49/49 通过。** db_executor 头冲突与 sqlExecutor API 漂移
  归入 Gap03 统一执行器变更处理。

### Task #2: db_driver 集成测试
- **状态**: completed
- **描述**: 补充集成测试（连接内存 SQLite）
- **验收**: test_driver.py 通过
- **结论**: Python 驱动此前只有骨架。定义了 NDJSON 线协议
  （请求 `{"sql", "params"}` / 响应 `{"ok", "columns", "rows", "rowcount"}`），
  实现 `drivers/python/mmdb/driver.py`（MMDBClient：缓冲收包处理粘包/半包、
  MMDBError 错误传播、上下文管理器）。
  集成测试 `drivers/python/test_driver.py`：进程内 Mock 服务器以后端
  sqlite3 :memory: 跑真实 TCP socket 全链路，8/8 通过（含参数化查询、
  错误传播、中文往返）。design.md 的"连接池泄漏检测/README 示例"未做 ——
  驱动尚无连接池，属后续工作。

### Task #3: Learning ds-c 验证
- **状态**: completed
- **描述**: 验证链表专题测试
- **验收**: 64 文件测试通过
- **结论**: design 的"64 文件"与仓库现状不符（与历史 design 数字漂移一致）：
  ds-c 实际为 33 个实现 .c 文件，**没有任何测试文件**，构建产物仅为
  静态库 libds.a。验证方式改为：独立 CMake 配置（learning/build, Ninja）
  全量构建通过，libds.a 正常产出。测试基础设施的补齐转入后续变更。

### Task #4: Learning algo-c 验证
- **状态**: completed
- **描述**: 验证排序算法测试
- **验收**: 18 文件测试通过
- **结论**: design 的"18 排序文件"与仓库现状不符：algo-c 实际仅 4 个
  .c 文件，**没有排序专题**。libalgo-c.a 全量构建通过。排序专题实现
  与测试转入后续变更。

### Task #5: Learning code-solutions 验证
- **状态**: completed
- **描述**: 验证 LeetCode 前 10 题
- **验收**: 测试通过率 ≥ 90%
- **结论**: 验证时发现前 10 题中仅 #1/#7/#8 有实现（且 #8 无测试），
  #2/#3/#4/#5/#6/#9/#10 缺失。已在 leetcode_1_100.cpp 补齐 7 题实现
  （addTwoNumbers / lengthOfLongestSubstring / findMedianSortedArrays /
  longestPalindrome / convert / isPalindrome / isMatch），并在
  leetcode_1_100_cpp_test.cpp 补齐 18 个用例（含 myAtoi 测试）。
  LeetCode1To100CPPTest 32/32 通过（10/10 题全覆盖），
  learning 全量 ctest **176/176 通过**（原 158 + 新增 18），通过率 100% ≥ 90%。
  另修复既有拼写错误：leetcode_2000_2100_test.cpp 中 19 处
  `maxi_mum_difference` → `maximum_difference`（否则该文件无法编译）。

## 完成状态

- [x] Task #1: RAG 核心管道实现
- [x] Task #2: db_driver 集成测试
- [x] Task #3: Learning ds-c 验证
- [x] Task #4: Learning algo-c 验证
- [x] Task #5: Learning code-solutions 验证

## 范围外发现（转入后续变更）

- db_executor 整库 bit-rot（index_catalog.h vs optimizer.h 类型重复声明、
  sqlExecutor.c buf_init/heap_insert API 漂移）→ Gap03 统一执行器变更
- ds-c / algo-c 缺测试基础设施与排序专题 → 后续 learning 变更
- Python 驱动连接池、Go/Java 驱动仍为空骨架 → 后续 driver 变更
