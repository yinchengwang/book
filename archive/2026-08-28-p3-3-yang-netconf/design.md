# P3-3 YANG/NETCONF 设计

> 日期：2026-08-28
> 状态：与提案一致，实现完成
> 目标：实现简化的 YANG 模型解析与 NETCONF 服务器

## 一、架构

```
YANG 模块
  ├─ Lexer (yang_lexer_next_token)
  ├─ Parser (yang_parse_module → 构建 Module AST)
  └─ Data Tree (yang_data_create/insert/get/delete)
        ↓
NETCONF Server (in-memory)
  ├─ RPC handler: <get>, <get-config>, <edit-config>, <lock>, <unlock>, <close-session>
  ├─ Config datastore: 引用 YANG Data Tree
  └─ chunked framing (占位)
```

## 二、YANG 模型

### 数据结构

```c
typedef struct yang_module_s yang_module_t;   // YANG module AST
typedef struct yang_node_s yang_node_t;       // AST 节点（container/leaf/list）
typedef struct yang_data_node_s yang_data_node_t;  // 数据实例节点
```

### API

- `yang_parse_module(text, len)` 解析 YANG 文本
- `yang_module_lookup(module, path)` 查找节点
- `yang_data_create(module)` 创建数据树
- `yang_data_insert(tree, path, value)` 插入数据
- `yang_data_get(tree, path, buf, len)` 读取数据

## 三、NETCONF

### 数据结构

```c
typedef struct netconf_server_s netconf_server_t;
typedef struct netconf_session_s netconf_session_t;
```

### RPC 处理

支持的 RPC：
- `<get>` — 读取运行 + 启动配置
- `<get-config>` — 读取指定 datastore
- `<edit-config>` — 修改配置（merge/replace/create/delete 操作）
- `<lock>` / `<unlock>` — 配置库锁
- `<close-session>` — 关闭会话

### Chunked framing (RFC 6242)

`netconn_frame_chunked()` 当前为占位（C2-5 T10），未实现 `\n#<size>\n` 分隔符解析。

## 四、文件清单

创建：
- `engineering/include/db/yang/yang_model.h` (159 行)
- `engineering/include/db/yang/yang_data.h` (244 行)
- `engineering/src/db/yang/yang_model.c` (599 行)
- `engineering/src/db/yang/yang_data.c` (455 行)
- `engineering/src/db/yang/yang_import.c` (12 行，占位)
- `engineering/include/db/netconf/netconf_server.h` (238 行)
- `engineering/src/db/netconf/netconf_server.c` (630 行)
- `engineering/src/db/netconf/netconf_chunked.c` (13 行，占位)
- `engineering/test/db/yang/yang_test.cpp` (563 行)

构建集成：
- `engineering/src/db/yang/CMakeLists.txt`
- `engineering/src/db/netconf/CMakeLists.txt`
- `engineering/test/db/yang/CMakeLists.txt`
- `engineering/src/db/CMakeLists.txt` (add_subdirectory yang + netconf)
- `engineering/test/db/CMakeLists.txt` (add_subdirectory yang)