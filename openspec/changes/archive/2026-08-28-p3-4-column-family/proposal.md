# P3-4 Column Family 提案

## 背景

P3-4 Column Family 是多模态能力补齐系列中支持列族存储模型的关键模块。列族存储（Column Family Store）是一种 NoSQL 数据模型，以列族（Column Family）为基本组织单位，支持动态列和宽行。

## 变更范围

### 新增文件
| 文件 | 说明 |
|------|------|
| `engineering/include/db/cf/cf_engine.h` | 列族引擎接口 |
| `engineering/src/db/cf/cf_engine.c` | 列族引擎实现 |
| `engineering/include/db/cf/cf_row.h` | 列族行结构 |
| `engineering/src/db/cf/cf_row.c` | 行操作实现 |
| `engineering/include/db/cf/cf_column.h` | 列定义结构 |
| `engineering/src/db/cf/cf_column.c` | 列操作实现 |
| `engineering/test/db/cf/cf_engine_test.cpp` | 列族测试 |

### 修改文件
| 文件 | 说明 |
|------|------|
| `engineering/src/db/CMakeLists.txt` | 注册 cf 模块 |

## 核心功能

1. **列族数据模型**
   - Column Family（列族）作为一级组织
   - Column（列）作为二级组织
   - Row Key（行键）唯一标识行
   - 支持动态列（Dynamic Columns）

2. **列类型支持**
   - Counter（计数器）
   - Expiring（TTL 列）
   - Static（静态列）
   - Complex（复合类型列）

3. **CRUD 操作**
   - Insert/Update（插入/更新）
   - Get（读取）
   - Delete（删除）
   - Batch（批量操作）

4. **存储引擎集成**
   - 与现有 KV 引擎结合
   - 支持 WAL 日志
   - 支持 MVCC 事务

## 验收标准

- [ ] 列族创建和删除正确
- [ ] 行插入和查询正确
- [ ] 批量操作正确
- [ ] 测试用例通过

## 风险与缓解

| 风险 | 缓解 |
|------|------|
| 动态列性能 | 内部使用 KV 存储映射 |
| 宽行存储 | 页面分裂策略 |
