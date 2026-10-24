# P2-2 文档聚合管道 提案

## 背景

多模态能力补齐系列中，P2-2 文档聚合管道是实现复杂文档查询的关键模块。支持对文档集合进行过滤、分组、聚合等操作，实现类似 SQL 的聚合功能。

## 变更范围

### 新增文件
| 文件 | 说明 |
|------|------|
| `engineering/include/db/doc/doc_aggregation.h` | 文档聚合接口 |
| `engineering/src/db/doc/doc_aggregation.c` | 聚合实现 |
| `engineering/test/db/doc/doc_aggregation_test.cpp` | 聚合测试 |

### 修改文件
| 文件 | 说明 |
|------|------|
| `engineering/src/db/doc/CMakeLists.txt` | 注册聚合模块 |

## 核心功能

1. **聚合操作符**
   - `$match`：文档过滤（类似 SQL WHERE）
   - `$group`：文档分组
   - `$sort`：结果排序
   - `$limit`/`$skip`：分页

2. **聚合管道**
   - 链式操作符组合
   - 内存/流式处理切换
   - 并行执行支持

3. **表达式计算**
   - JSONPath 字段访问
   - 算术表达式求值
   - 字符串操作

## 验收标准

- [ ] $match 过滤测试通过
- [ ] $group 分组测试通过
- [ ] 管道链式执行正确
- [ ] 表达式计算正确

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 复杂管道性能 | 使用迭代器模式，流式处理 |
| 内存溢出 | 限制中间结果大小 |
