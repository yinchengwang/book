# 工程对照笔记：Redis 对象系统

## engineering/src/redis/object.c 对照

### 核心结构

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `redisObject` | `redisObject` | 对象头结构 |
| `sds` | `sds` | 简单动态字符串 |
| `OBJ_STRING` | `OBJ_STRING` | 对象类型常量 |

### 关键函数对照

| 学习代码 | 工程代码 | 说明 |
|---------|---------|------|
| `obj_type_name()` | `strtype()` | 类型名称 |
| `encoding_name()` | `strencoding()` | 编码名称 |

### 工程实现要点

1. **robj 结构**
   ```c
   // object.c
   typedef struct redisObject {
       unsigned type:4;          // 4bit
       unsigned encoding:4;     // 4bit
       unsigned lru:LRU_BITS;   // 24bit
       int refcount;             // 引用计数
       void *ptr;                // 指向实际数据
   } robj;
   ```

2. **类型检查宏**
   ```c
   #define checkType(c,o,t) \
       if (o->type != t) return (c) ? addReply(c,wrongtypeerr) : 0
   ```

3. **对象创建**
   ```c
   robj *createObject(int type, void *ptr) {
       robj *o = zmalloc(sizeof(*o));
       o->type = type;
       o->encoding = OBJ_ENCODING_RAW;
       o->ptr = ptr;
       o->refcount = 1;
       o->lru = LRU_CLOCK();
       return o;
   }
   ```

4. **引用计数**
   ```c
   void incrRefCount(robj *o) { o->refcount++; }
   void decrRefCount(robj *o) {
       if (--o->refcount == 0) zfree(o);
   }
   ```

5. **编码转换时机**
   ```c
   // String 追加超过阈值 -> int -> embstr -> raw
   // List 元素超过 512 -> ziplist -> linkedlist
   // Hash 字段超过 512 -> ziplist -> hashtable
   // ZSet 元素超过 128 -> ziplist -> skiplist
   ```

### 扩展阅读

- Redis 源码：`src/object.c`
- `src/server.h` - robj 定义
- `src/sds.h` - SDS 字符串
- `src/ziplist.c` - 压缩列表
- `src/intset.c` - 整数集合
- `src/t_zset.c` - ZSet 实现
