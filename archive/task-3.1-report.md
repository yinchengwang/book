# Task 3.1 统计收集器核心实现

## STATUS: DONE

## COMMITS
- `285e37e7fe004641f359054c75ff989c355ae986` — feat(stats): 实现统计收集器核心

## 变更文件清单
| 文件 | 操作 |
|------|------|
| `engineering/include/db/tools/stats.h` | 扩展：空占位 → 完整接口（含 StatDatabase/StatTable/StatIndex 结构体及 stats_init/shutdown/get/reset API） |
| `engineering/src/db/tools/stats/stats.c` | 实现：统计收集器核心逻辑（含内部 TableEntry/IndexEntry 管理） |
| `engineering/test/db/tools/test_stats.cpp` | 新增：8 个测试用例 |
| `engineering/test/db/tools/CMakeLists.txt` | 更新：添加 test_stats 目标 |

## TESTS

```
[==========] Running 8 tests from 1 test suite.
[----------] 8 tests from StatsTest
[ RUN      ] StatsTest.InitShutdown       -- OK
[ RUN      ] StatsTest.GetDatabase        -- OK
[ RUN      ] StatsTest.GetDatabaseWithNull -- OK
[ RUN      ] StatsTest.GetTablesEmpty     -- OK
[ RUN      ] StatsTest.GetIndexesEmpty    -- OK
[ RUN      ] StatsTest.Reset              -- OK
[ RUN      ] StatsTest.NullCollector      -- OK
[ RUN      ] StatsTest.ResetNull          -- OK
[----------] 8 tests ran (2 ms total)
[  PASSED  ] 8 tests.
```

## 覆盖率说明

测试覆盖了：
- 正常路径：初始化/关闭、数据库级统计查询
- 边界条件：空表/空索引查询
- 错误路径：NULL 参数、NULL 收集器
- 特殊操作：统计重置、重置空收集器不崩溃