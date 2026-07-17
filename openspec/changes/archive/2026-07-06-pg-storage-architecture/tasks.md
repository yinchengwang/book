# PostgreSQL 风格存储架构实现任务

## M1: Catalog 系统

- [x] 1.1 创建 `include/db/catalog.h` 公共头文件
- [x] 1.2 创建 `include/db/catalog_defs.h` 系统表定义
- [x] 1.3 实现 pg_class 系统表管理
- [x] 1.4 实现 pg_attribute 系统表管理
- [x] 1.5 实现 pg_index 系统表管理
- [x] 1.6 实现 Catalog Cache 缓存
- [x] 1.7 实现表创建/删除的 Catalog 更新
- [x] 1.8 实现列操作（添加/删除列）
- [x] 1.9 实现索引操作的 Catalog 更新
- [x] 1.10 创建 `src/db/storage/catalog.c` 实现

## M2: Buffer Pool

- [x] 2.1 创建 `include/db/buf.h` 公共头文件
- [x] 2.2 创建 `include/db/buf_internals.h` 内部结构
- [x] 2.3 实现 BufferDesc 结构
- [x] 2.4 实现 Buffer Pool 初始化
- [x] 2.5 实现 Hash 表管理（快速查找）
- [x] 2.6 实现 Clock-Sweep 置换算法
- [x] 2.7 实现 ReadBuffer 页面读取
- [x] 2.8 实现 NewBuffer 新页面分配
- [x] 2.9 实现 Buffer Pin/Unpin
- [x] 2.10 实现脏页标记和刷盘
- [x] 2.11 实现与 KV 存储的集成
- [x] 2.12 创建 `src/db/storage/bufmgr.c` 实现

## M3: Access Method 层

- [x] 3.1 创建 `include/db/rel.h` Relation 定义
- [x] 3.2 创建 `include/db/am.h` AM 接口定义
- [x] 3.3 实现 RelationData 结构
- [x] 3.4 实现 relation_open/close
- [x] 3.5 实现 relation_create/drop
- [x] 3.6 实现 TupleDesc 管理
- [x] 3.7 实现 ScanKey 和扫描接口
- [x] 3.8 创建 `src/db/storage/rel.c` 实现

## M4: Heap AM

- [x] 4.1 创建 `include/db/heapam.h` Heap AM 接口
- [x] 4.2 创建 `include/db/page.h` Page 工具函数
- [x] 4.3 实现 PageHeader 和 LinePointer 结构
- [x] 4.4 实现 Page 初始化和操作
- [x] 4.5 实现 heap_insert 元组插入
- [x] 4.6 实现 heap_getnext 顺序扫描
- [x] 4.7 实现 heap_update 元组更新
- [x] 4.8 实现 heap_delete 元组删除
- [x] 4.9 实现表扫描描述符管理
- [x] 4.10 实现 IndexScan 索引扫描
- [x] 4.11 创建 `src/db/storage/heapam.c` 实现
- [x] 4.12 创建 `src/db/storage/page.c` 实现

## M5: BTree AM

- [x] 5.1 创建 `include/db/btreeam.h` BTree AM 接口
- [x] 5.2 实现 BTree Page 结构
- [x] 5.3 实现 BTree 键比较函数
- [x] 5.4 实现 btbuild 索引构建
- [x] 5.5 实现 btinsert 索引插入
- [x] 5.6 实现 btgetnext 索引扫描
- [x] 5.7 实现 BTree 页面分裂
- [x] 5.8 实现 BTree 根页面管理
- [x] 5.9 创建 `src/db/storage/btreeam.c` 实现

## M6: 与现有系统集成

- [x] 6.1 将 Buffer Pool 集成到 KV 存储
- [x] 6.2 将 Catalog 集成到 SQL 层
- [x] 6.3 将 AM 层与锁系统连接
- [x] 6.4 实现 WAL 与 Buffer 的协调（新增 wal_buf.c/h）
- [x] 6.5 更新 SQL 执行器使用新架构（新增 sql_executor.c/h）

## M7: 测试

- [x] 7.1 创建 Catalog 单元测试
- [x] 7.2 创建 Buffer Pool 单元测试
- [x] 7.3 创建 Heap AM 单元测试
- [x] 7.4 创建 BTree AM 单元测试
- [x] 7.5 创建集成测试（storage_integration_test.cpp）
- [x] 7.6 运行所有测试验证

## M8: 文档

- [x] 8.1 添加存储架构文档
- [x] 8.2 更新 CLAUDE.md 说明新架构
- [x] 8.3 添加 API 使用示例
