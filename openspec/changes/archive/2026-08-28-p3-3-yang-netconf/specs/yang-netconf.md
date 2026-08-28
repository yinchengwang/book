# P3-3 YANG/NETCONF 规格

## 能力: yang-model

YANG 模型解析（RFC 6020 简化子集）。

### ADDED Requirements

#### Requirement: YANG 模块解析
系统 SHALL 提供 YANG 模块文本解析：
- `yang_parse_module(text, len)` 返回 `yang_module_t*`
- 解析 container / leaf / leaf-list / list / choice 等核心语句
- 识别 typedef / import / include（简化）

#### Requirement: YANG 数据树
系统 SHALL 提供数据实例树：
- `yang_data_create(module)` 创建数据树
- `yang_data_insert(tree, path, value)` 插入
- `yang_data_get(tree, path, buf, len)` 读取
- `yang_data_delete(tree, path)` 删除

#### Requirement: YANG 验证
数据修改 SHALL 校验路径与 module 定义匹配（类型、必填、范围）。

#### Scenario: 解析简单 YANG
- **WHEN** 用户提交 `module foo { container x { leaf y { type string; } } }`
- **THEN** `yang_parse_module` 返回包含 container x 和 leaf y 的 module

#### Scenario: 插入并读取数据
- **WHEN** 用户 `yang_data_insert(tree, "/x/y", "hello")`
- **THEN** `yang_data_get` 返回 "hello"

## 能力: netconf-server

NETCONF 服务器（RFC 6241 简化子集）。

### ADDED Requirements

#### Requirement: NETCONF RPC 处理
系统 SHALL 实现核心 RPC：
- `<get>` 读取 running 配置
- `<get-config>` 读取指定 datastore
- `<edit-config>` 修改配置（merge/replace/create/delete 操作）
- `<lock>` / `<unlock>` 锁定/解锁 datastore
- `<close-session>` 关闭会话

#### Requirement: 配置 datastore
系统 SHALL 维护 running 配置 datastore，基于 YANG data tree。

#### Scenario: get-config RPC
- **WHEN** 客户端发送 `<get-config><source><running/></source></get-config>`
- **THEN** 服务器返回当前 running 配置的 XML

#### Scenario: edit-config merge
- **WHEN** 客户端发送 `<edit-config><target><running/></target><config>...</config><default-operation>merge</default-operation></edit-config>`
- **THEN** 配置按 merge 语义合并到 running datastore

## 已知限制

- `yang_parse_import` 当前为占位（仅日志输出），未实现完整 import/module 引用解析
- `netconn_frame_chunked` 当前为占位，未实现 RFC 6242 chunked framing 解析
- 整体链路未在 ctest 中验证（db_core 三个预存错误阻塞）