# redis

Redis 核心数据结构的手写实现。纯 C，输出到静态库。

## 源文件

| 文件 | 说明 |
|------|------|
| `redis_adlist.c` | 双向链表 |
| `redis_sds.c` | 简单动态字符串 |
| `redis_skiplist.c` | 跳表 |
| `redis_zmalloc.c` | 内存分配 |
| `util.h` | 工具宏 |
| `sdsalloc.h` | 分配器配置 |

## 依赖

无外部依赖，无链接 `project_includes`（include 通过显式 `include_directories` 添加）。

## 构建方式

GLOB 收集当前目录下所有 `.c` 文件，不递归子目录。