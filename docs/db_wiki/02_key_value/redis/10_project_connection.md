# Redis 项目关联

## 学习目标

- 分析 Redis 设计对项目的启发
- 对比项目中 Redis 相关实现

## 项目中已有的 Redis 实现

项目在 `engineering/src/redis/` 中已有 Redis 核心组件移植：

| 组件 | 状态 | 说明 |
|------|------|------|
| SDS | ✅ 完成 | `src/redis/sds.c` |
| 双向链表 | ✅ 完成 | `src/redis/adlist.c` |
| 跳表 | ✅ 完成 | `src/redis/skiplist.c` |
| ZipList | 📝 待实现 | |
| QuickList | 📝 待实现 | |
| 事件循环 | 📝 待实现 | |
| 协议解析 | 📝 待实现 | |

## SDS 在项目中的应用

项目中 SDS 的使用模式：

```c
// 项目中的 SDS 实现
// engineering/src/redis/sds.c
// 核心结构
typedef char *sds;

struct sdshdr {
    unsigned int len;    // 已用长度
    unsigned int free;   // 空闲长度
    char buf[];          // 数据数组
};

// 优势
// 1. O(1) 长度获取 — 相比 strlen O(n)
// 2. 预分配策略 — sdsMakeRoomFor
// 3. 二进制安全 — 以 len 而非 '\0' 判断结尾
```

## 跳表在项目中的应用

```c
// 项目中的跳表实现
// engineering/src/redis/skiplist.c
// 用于有序集合和范围查询
typedef struct zskiplist {
    struct zskiplistNode *header, *tail;
    unsigned long length;
    int level;
} zskiplist;
```

## 可借鉴的设计

| Redis 设计 | 项目借鉴 |
|-----------|---------|
| 事件循环 AE | 可移植到异步 IO 处理 |
| SDS 字符串 | 已在项目中使用 |
| 跳表 | 已在项目中使用 |
| 渐进式 rehash | 哈希表扩容策略 |
| 对象池 | 减少内存分配 |

## 要点总结

- 项目中已移植 SDS、跳表等 Redis 核心组件
- 事件循环和协议解析可继续实现
- Redis 的渐进式 rehash 设计值得借鉴
- 单线程事件循环模型适合 IO 密集型场景

## 思考题

1. 项目中已有的 SDS 和跳表实现与 Redis 源码有哪些差异？
2. 如果需要实现一个简化版 Redis，需要完成哪些核心组件？
3. 项目的存储引擎是否可以借鉴 Redis 的持久化策略？