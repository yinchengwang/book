# storage_overview — 存储引擎架构概览

## 核心组件
- Catalog：系统表管理，OID 分配
- Buffer Pool：内存缓存，Clock-Sweep 置换
- Heap：堆表存储，页面布局
- BTree：索引结构，键值定位

## 编译运行
make && ./test
