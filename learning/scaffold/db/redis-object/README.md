# Redis 对象系统

## 概述

演示 Redis 的 5 种对象类型和内部编码：
- **String**: int / embstr / raw
- **List**: linkedlist / ziplist
- **Hash**: hashtable / ziplist
- **Set**: intset / hashtable
- **ZSet**: skiplist / ziplist

## 编译运行

```bash
make
./redis_object
```

## 对象类型

| 类型 | 用途 | 编码选项 |
|------|------|----------|
| String | 字符串/计数器 | int, embstr, raw |
| List | 列表/队列 | linkedlist, ziplist |
| Hash | 哈希表 | hashtable, ziplist |
| Set | 集合 | intset, hashtable |
| ZSet | 有序集合 | skiplist, ziplist |

## 文件结构

```
redis-object/
├── main.c      # 对象系统演示
├── Makefile
├── README.md
└── NOTES.md
```
