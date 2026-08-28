# P3-4 Column Family 规格

## 能力: column-family

列族（Column Family）存储引擎，类 Cassandra/HBase 数据模型。

### ADDED Requirements

#### Requirement: 列族 CRUD
系统 SHALL 提供列族的创建、删除、列表操作：
- `cf_create_family(db, name)` 创建列族
- `cf_drop_family(db, name)` 删除列族
- `cf_list_families(db, names, max)` 列出所有列族

#### Requirement: 行级别读写
系统 SHALL 提供行级别和列级别两种粒度的读写：
- `cf_put(db, cf, key, col, val)` 写入单列
- `cf_get(db, cf, key, col, buf, len)` 读取单列
- `cf_get_row(db, cf, key, out_row)` 读取整行
- `cf_delete_row(db, cf, key)` 删除整行
- `cf_delete_column(db, cf, key, col)` 删除单列

#### Requirement: 动态列
列族 SHALL 支持动态列（同一行可有不同列集合），列名运行时确定。

#### Requirement: 批量操作
系统 SHALL 提供批量原子操作：`cf_batch_execute(db, ops, n)`，
批量内所有操作要么全部成功要么全部失败。

#### Requirement: 行迭代器
系统 SHALL 提供行迭代器：
- `cf_iter_create(db, cf, start_key, end_key)`
- `cf_iter_next(iter)` 推进
- `cf_iter_free(iter)` 释放

#### Requirement: 持久化与恢复
系统 SHALL 将列族数据持久化到磁盘（基于底层 KV），并在 `cf_open` 时自动恢复。

#### Scenario: 创建列族并写入
- **WHEN** 用户调用 `cf_create_family(db, "users")` 然后 `cf_put(db, "users", "u1", "name", "alice")`
- **THEN** 数据持久化到磁盘，重新打开后仍可读取

#### Scenario: 批量原子操作
- **WHEN** 用户提交批量操作 [put u1, put u2, delete u3]
- **THEN** 全部三个操作作为一个事务执行，失败时全部回滚

#### Scenario: 动态列
- **WHEN** 用户给同一行写入不同列名（name/age/email）
- **THEN** `cf_get_row` 返回所有已写入列