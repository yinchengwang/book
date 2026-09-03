# Task 5 报告：COPY 工具 JSON 和 SQL 格式支持

## STATUS: DONE

## 实现内容

### 1. 新增文件

- `engineering/src/db/tools/copy/copy_json.c` — JSON Lines 格式实现
  - `json_write_row()`: 导出 JSON 行，支持转义双引号、反斜杠、换行、制表符等
  - `json_parse_line()`: 解析 JSON 行，支持字符串、数字、布尔值、null 值

- `engineering/src/db/tools/copy/copy_sql.c` — SQL INSERT 格式实现
  - `sql_write_insert()`: 生成 INSERT 语句，单引号转义为两个单引号

- `engineering/test/db/tools/test_copy_json.cpp` — JSON 格式测试（15 个用例）
- `engineering/test/db/tools/test_copy_sql.cpp` — SQL 格式测试（8 个用例）

### 2. 修改文件

- `engineering/include/db/tools/copy.h` — 添加 JSON 和 SQL 格式函数声明
- `engineering/src/db/tools/copy/CMakeLists.txt` — 注册新源码文件
- `engineering/test/db/tools/CMakeLists.txt` — 注册新测试目标

## 测试结果

```
test_copy_json: 15/15 通过
  - CopyJsonTest: 7 个导出测试
  - CopyJsonParseTest: 8 个解析测试

test_copy_sql: 8/8 通过
  - CopySqlTest: 8 个导出测试
```

## COMMITS

```
d8b60e08 feat(copy): 添加 JSON 和 SQL 格式支持
```

## 功能说明

### JSON Lines 格式

每行一个 JSON 对象，适合流式处理：

```json
{"id":1,"name":"Alice","age":30}
{"id":2,"name":"Bob","age":null}
```

### SQL INSERT 格式

生成可执行的 INSERT 语句，便于数据库迁移或审计：

```sql
INSERT INTO users (id, name, age) VALUES (1, 'Alice', 30);
INSERT INTO users (id, name, age) VALUES (2, 'Bob', NULL);
```