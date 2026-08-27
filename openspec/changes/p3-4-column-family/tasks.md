# P3-4 Column Family 任务清单

## 任务列表

### Task #1: 创建列族引擎头文件
- **状态**: pending
- **预估工时**: 1h
- **描述**: 定义列族引擎的公共 API
- **验收标准**: 头文件语法正确
- **实现文件**:
  - `engineering/include/db/cf/cf_engine.h`
  - `engineering/include/db/cf/cf_row.h`
  - `engineering/include/db/cf/cf_column.h`

### Task #2: 实现列族行结构
- **状态**: pending
- **预估工时**: 2h
- **依赖**: Task #1
- **描述**: 实现行键和列值存储
- **验收标准**: 行结构正确
- **实现**: `cf_row_create()`, `cf_row_set()`, `cf_row_get()`

### Task #3: 实现列定义结构
- **状态**: pending
- **预估工时**: 1.5h
- **依赖**: Task #1
- **描述**: 实现列定义和类型
- **验收标准**: 列定义正确
- **实现**: `cf_column_define()`, `cf_column_type_t`

### Task #4: 实现列族引擎
- **状态**: pending
- **预估工时**: 3h
- **依赖**: Task #2-3
- **描述**: 实现列族 CRUD 操作
- **验收标准**: CRUD 操作正确
- **实现**: `cf_engine_open()`, `cf_engine_insert()`, `cf_engine_get()`, `cf_engine_delete()`

### Task #5: 实现批量操作
- **状态**: pending
- **预估工时**: 2h
- **依赖**: Task #4
- **描述**: 实现批量读写操作
- **验收标准**: 批量操作正确
- **实现**: `cf_engine_batch_insert()`, `cf_engine_batch_get()`

### Task #6: 实现列族元数据管理
- **状态**: pending
- **预估工时**: 1.5h
- **依赖**: Task #4
- **描述**: 实现列族元数据管理
- **验收标准**: 元数据管理正确
- **实现**: `cf_create_family()`, `cf_drop_family()`, `cf_list_families()`

### Task #7: 编写 GoogleTest 测试用例
- **状态**: pending
- **预估工时**: 2h
- **依赖**: Task #4-6
- **描述**: 为列族引擎编写测试
- **验收标准**: 所有测试通过
- **实现文件**: `engineering/test/db/cf/cf_engine_test.cpp`

### Task #8: 更新 CMakeLists.txt
- **状态**: pending
- **预估工时**: 0.5h
- **依赖**: Task #7
- **描述**: 注册列族模块
- **验收标准**: 编译通过
- **实现文件**: `engineering/src/db/CMakeLists.txt`

## 预估总工时

约 13.5 小时（2 天）

## 实现约束

1. 列族内部使用 KV 存储实现
2. 行键使用字符串，列名使用字符串
3. 列值支持二进制数据
4. 不实现分布式（单节点）
