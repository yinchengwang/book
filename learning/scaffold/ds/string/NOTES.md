# 工程对照笔记

## 概述

本学习卡片演示的字符串概念直接对应工程代码中的实际应用。以下对照工程源码中的字符串处理模式，帮助理解理论与实践的关联。

---

## 对照一：Redis SDS 动态字符串

**源文件**：`engineering/src/redis/redis_sds.c`

Redis SDS（Simple Dynamic Strings）是 Redis 的核心字符串实现，比 C 字符串更高效：

```c
/* SDS 存储结构（内存布局）
 * +---------+---------+---------+---------+---+----+
 * |   len   |  alloc  |  flags  |   data...   | \0 |
 * +---------+---------+---------+---------+---+----+
 * ^         ^         ^         ^             ^
 * sh        sh+1      sh+2      s (返回给用户)  s+len
 */
redis_sds _redis_sds_new_len(const void *init, size_t init_len, int try_malloc)
{
    char type = redis_sds_req_type(init_len);
    int hdr_len = redis_sds_hdr_size(type);
    /* 一次性分配：header + 数据 + '\0' */
    sh = try_malloc ? s_trymalloc_usable(malloc_size, &usable)
                    : s_malloc_usable(malloc_size, &usable);
    s = sh + hdr_len;
    s[init_len] = '\0';
    return s;
}
```

**对照学习点**：
- SDS 头中直接存储 `len`，`strlen()` 是 O(1)，不是 O(n)
- 预分配策略避免每次追加都重新分配（类似本卡片的 DynString，但 SDS 更高效）
- 支持 5 种头部类型（SDS_TYPE_5/8/16/32/64），根据字符串长度选择最小头部

---

## 对照二：dict 的字符串工具函数

**源文件**：`engineering/src/algo-prod/dict/dict_core.c`

分词器的 dict 模块实现了健壮的字符串操作：

```c
/* 安全字符串复制 */
char *dict_strdup(const char *text)
{
    size_t length = strlen(text);
    char *copy = (char *)malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, text, length + 1);  /* 包括 '\0' */
    return copy;
}

/* 字符串切片复制 */
char *dict_strdup_slice(const char *text, int32_t byte_start, int32_t byte_length)
{
    char *copy = (char *)malloc((size_t)byte_length + 1);
    if (!copy) return NULL;
    memcpy(copy, text + byte_start, (size_t)byte_length);
    copy[byte_length] = '\0';  /* 手动加 '\0' */
    return copy;
}
```

**对照学习点**：
- `dict_strdup_slice` 用于处理 UTF-8 字符串的字节级切片
- 必须手动添加 `'\0'`，这正是本卡片强调的陷阱
- `strlen()` 用于获取长度，与本卡片的用法一致

---

## 对照三：UTF-8 解码

**源文件**：`engineering/src/algo-prod/dict/dict_core.c`

```c
/* UTF-8 单字符解码 */
int dict_utf8_decode_one(const char *text, int32_t remaining,
                         uint32_t *codepoint, int32_t *byte_length)
{
    const unsigned char *bytes = (const unsigned char *)text;

    if (bytes[0] < 0x80) {                    /* ASCII: 1 字节 */
        *codepoint = bytes[0];
        *byte_length = 1;
    } else if ((bytes[0] & 0xE0) == 0xC0) {   /* 2 字节: 110xxxxx */
        *codepoint = ((uint32_t)(bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
        *byte_length = 2;
    } else if ((bytes[0] & 0xF0) == 0xE0) {   /* 3 字节: 1110xxxx (中文) */
        *codepoint = ((uint32_t)(bytes[0] & 0x0F) << 12) |
                     ((uint32_t)(bytes[1] & 0x3F) << 6) |
                     (bytes[2] & 0x3F);
        *byte_length = 3;
    }
    /* ... */
}
```

**对照学习点**：
- 这是本卡片 demo_utf8_awareness 的工程实现版本
- UTF-8 解码是文本处理的基础（分词、搜索、比较都依赖它）
- 中文字符的 UTF-8 编码确实是 3 字节，与本卡片描述一致

---

## 对照四：SDS 与 dict_strdup 的区别

| 特性 | Redis SDS | dict_strdup |
|------|-----------|-------------|
| 存储内容 | 字符串数据 + 元数据（len, alloc） | 仅字符串数据 |
| 长度获取 | O(1)（从头部读取） | O(n)（strlen 遍历） |
| 追加操作 | 预分配 + 快速追加 | 需重新分配 |
| 适用场景 | 高频字符串操作 | 一次性字符串复制 |

---

## 工程代码模式总结

1. **SDS 模式**：预分配 + O(1) strlen，适用于高频追加场景（Redis）
2. **dict_strdup 模式**：按需分配 + memcpy，适用于一次性使用场景（分词器）
3. **UTF-8 解码模式**：逐字节分析编码前缀，适用于文本处理（中分分词）

---

## 延伸阅读

- `engineering/src/redis/redis_sds.c` - Redis SDS 完整实现
- `engineering/src/algo-prod/dict/dict_core.c` - 字符串工具与 UTF-8 解码
