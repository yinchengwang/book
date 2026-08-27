# P3-3 YANG/NETCONF 任务清单

## 任务列表

### Task #1: 创建 YANG 模块头文件
- **状态**: pending
- **预估工时**: 1h
- **描述**: 定义 YANG 模型解析的公共 API
- **验收标准**: 头文件语法正确
- **实现文件**:
  - `engineering/include/db/yang/yang_model.h`
  - `engineering/include/db/yang/yang_data.h`

### Task #2: 实现 YANG 词法分析器
- **状态**: pending
- **预估工时**: 2h
- **依赖**: Task #1
- **描述**: 实现 YANG 关键字和符号识别
- **验收标准**: 词法分析正确
- **实现**: `yang_lexer_next_token()`, `yang_token_type_t`

### Task #3: 实现 YANG 语法分析器
- **状态**: pending
- **预估工时**: 3h
- **依赖**: Task #2
- **描述**: 实现 YANG 语句树构建
- **验收标准**: 语法分析正确
- **实现**: `yang_parse_module()`, `yang_statement_create()`

### Task #4: 实现 YANG 数据节点
- **状态**: pending
- **预估工时**: 2h
- **依赖**: Task #1
- **描述**: 实现数据节点结构操作
- **验收标准**: 数据节点正确创建
- **实现**: `yang_data_create()`, `yang_data_set()`, `yang_data_free()`

### Task #5: 实现 NETCONF 服务器头文件
- **状态**: pending
- **预估工时**: 1h
- **描述**: 定义 NETCONF 服务器接口
- **验收标准**: 头文件语法正确
- **实现文件**: `engineering/include/db/netconf/netconf_server.h`

### Task #6: 实现 NETCONF RPC 处理
- **状态**: pending
- **预估工时**: 3h
- **依赖**: Task #4-5
- **描述**: 实现 NETCONF RPC 操作
- **验收标准**: RPC 处理正确
- **实现**: `netconf_handle_get()`, `netconf_handle_get_config()`, `netconf_handle_edit_config()`

### Task #7: 实现配置数据存储
- **状态**: pending
- **预估工时**: 2h
- **依赖**: Task #4
- **描述**: 实现配置数据存储
- **验收标准**: 配置存储正确
- **实现**: `yang_datastore_t`, `yang_datastore_get()`, `yang_datastore_set()`

### Task #8: 编写 GoogleTest 测试用例
- **状态**: pending
- **预估工时**: 2h
- **依赖**: Task #2-7
- **描述**: 为 YANG/NETCONF 编写测试
- **验收标准**: 所有测试通过
- **实现文件**: `engineering/test/db/yang/yang_test.cpp`

### Task #9: 更新 CMakeLists.txt
- **状态**: pending
- **预估工时**: 0.5h
- **依赖**: Task #8
- **描述**: 注册 YANG/NETCONF 模块
- **验收标准**: 编译通过
- **实现文件**: `engineering/src/db/CMakeLists.txt`

## 预估总工时

约 16.5 小时（2-3 天）

## 实现约束

1. YANG 支持简化子集（不实现完整 RFC 6020）
2. NETCONF 使用简化的 in-memory 实现（不依赖 libnetconf）
3. XML 解析使用手写的简易解析器
