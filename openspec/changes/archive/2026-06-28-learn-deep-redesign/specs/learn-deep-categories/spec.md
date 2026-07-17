# learn-deep-categories

图解系列分类体系规格

## ADDED Requirements

### Requirement: 分类体系结构

系统 SHALL 定义 9 个一级分类，每个分类包含唯一标识、显示名称、slug 和文章数量。

#### Scenario: 分类数据定义
- **WHEN** 系统加载图解系列数据
- **THEN** 应包含以下分类：Linux(~80)、网络(~50)、系统(~30)、数据库(~30)、向量数据库(~30)、算法(~52)、C(~44)、Python(~43)、C++(~42)

### Requirement: 分类显示顺序

分类 SHALL 按照预定义顺序显示，用户 SHALL 能够通过折叠/展开操作隐藏分类列表。

#### Scenario: 分类折叠
- **WHEN** 用户点击分类标题
- **THEN** 该分类的文章列表展开或折叠

#### Scenario: 分类展开
- **WHEN** 页面加载时
- **THEN** 默认展开第一个分类，其他分类折叠

### Requirement: 文章归属

每篇文章 SHALL 归属到唯一分类，分类字段 SHALL 使用 categoryId 而非 category 名称。

#### Scenario: 文章分类显示
- **WHEN** 用户查看文章列表或详情
- **THEN** 显示文章的分类名称和 slug

### Requirement: 分类统计

系统 SHALL 在分类标题旁显示该分类的文章数量。

#### Scenario: 显示文章数
- **WHEN** 渲染分类列表
- **THEN** 每个分类标题后显示 "（N篇）"

## 分类映射规则

### Linux 分类（~80篇）
包含：Page Cache、IO 调度器、文件系统（ext4/xfs/btrfs）、进程管理、内存管理、perf/strace/htop、cgroup/namespace、io_uring、磁盘/RAID/LVM

### 网络分类（~50篇）
包含：TCP/IP、HTTP/1.1/2/3、HTTPS 握手、epoll、Reactor/Proactor、零拷贝、XDP、iptables、Socket

### 系统分类（~30篇）
包含：进程线程、锁机制、虚拟内存、CPU 缓存一致性、NUMA、调度算法、预读、软中断

### 数据库分类（~30篇）
包含：MySQL 存储引擎、Buffer Pool、WAL、索引（B+树/哈希）、事务（MVCC/锁）、Redis

### 向量数据库分类（~30篇）
包含：HNSW、IVF、PQ、DiskANN、Milvus、Pinecone、向量量化

### 算法分类（~52篇）
包含：排序算法、查找算法、图论、动态规划（保持不变）

### C 分类（~44篇）
包含：C 语法、指针、内存管理、文件 IO（保持不变）

### Python 分类（~43篇）
包含：Python 语法、数据结构、元编程（保持不变）

### C++ 分类（~42篇）
包含：C++ 特性、智能指针、多线程（保持不变）
