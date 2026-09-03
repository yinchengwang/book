# 工程对照笔记

## 概述

控制流是所有程序的基础结构。本卡片演示的模式在工程代码中有广泛应用，特别是在存储引擎、协议解析、网络处理等场景中。以下对照工程源码中的实际使用。

---

## 对照一：Parser 词法分析状态机

**源文件**：`engineering/src/db/sql/parser.y` 或相关词法分析代码

SQL Parser 使用状态机解析 SQL 语句：

```c
// 简化的 Token 解析状态机
typedef enum {
    LEX_START,
    LEX_IDENTIFIER,
    LEX_NUMBER,
    LEX_STRING,
    LEX_OPERATOR
} LexState;

Token next_token(const char **input) {
    LexState state = LEX_START;
    const char *start = *input;

    while (**input) {
        char ch = **input;

        switch (state) {
            case LEX_START:
                if (isalpha(ch)) {
                    state = LEX_IDENTIFIER;
                } else if (isdigit(ch)) {
                    state = LEX_NUMBER;
                } else if (ch == '\'') {
                    state = LEX_STRING;
                } else if (isoperator(ch)) {
                    return make_operator_token(ch);
                }
                break;

            case LEX_IDENTIFIER:
                if (!isalnum(ch) && ch != '_') {
                    return make_ident_token(start, *input - start);
                }
                break;

            case LEX_NUMBER:
                if (!isdigit(ch) && ch != '.') {
                    return make_number_token(start, *input - start);
                }
                break;

            // ... 更多状态
        }
        (*input)++;
    }
    return EOF_TOKEN;
}
```

**对照学习点**：
- 与本卡片 HTTP 解析状态机完全相同的模式
- 状态机使复杂解析逻辑变得清晰可维护
- 每个状态处理一个字符，根据字符决定下一个状态

---

## 对照二：WAL 刷盘错误处理

**源文件**：`engineering/src/db/storage/wal/wal.c`

WAL 模块使用 goto 进行错误处理清理：

```c
int wal_write(WALContext *ctx, const char *data, size_t len) {
    int result = -1;
    XLogRecord *record = NULL;
    XLogPage *page = NULL;
    void *buf = NULL;

    /* 1. 分配日志记录 */
    record = malloc(sizeof(XLogRecord));
    if (!record) goto cleanup;

    /* 2. 分配缓冲区 */
    buf = malloc(WAL_PAGE_SIZE);
    if (!buf) goto cleanup;

    /* 3. 写入页面 */
    page = wal_find_page(ctx, record->lsn);
    if (!page) goto cleanup;

    /* 4. 复制数据 */
    memcpy(page->data + record->offset, data, len);

    /* 5. 标记脏页 */
    if (wal_mark_dirty(ctx, page) != 0) goto cleanup;

    result = 0;

cleanup:
    if (buf) free(buf);
    if (record) free(record);
    return result;
}
```

**对照学习点**：
- 与本卡片 demo_goto_error_handling 完全相同的模式
- 每个资源分配后检查，失败则跳转到 cleanup
- cleanup 标签统一释放所有可能已分配的资源
- 避免了在每个错误点重复写释放代码

---

## 对照三：Buffer Pool 的页面置换循环

**源文件**：`engineering/src/db/storage/bufmgr.c`

Buffer Pool 的 Clock-Sweep 置换算法使用循环：

```c
// Clock-Sweep 页面置换算法
int buffer_get_page(BufferPool *pool, PageId page_id) {
    int frame_id = -1;

    /* 已在缓存中 */
    int existing = hash_lookup(pool->hash, page_id);
    if (existing >= 0) {
        PageDescriptor *desc = &pool->pages[existing];
        desc->refcount++;
        desc->flags &= ~BM_DIRTY;  /* 清除引用位供下次判断 */
        return existing;
    }

    /* 需要置换：Clock-Sweep 寻找victim */
    while (frame_id < 0) {
        PageDescriptor *desc = &pool->pages[pool->clock_hand];

        if (desc->refcount == 0 && !desc->pin_count) {
            /* 找到 victim */
            if (desc->flags & BM_DIRTY) {
                /* 需要写回磁盘 */
                disk_write_page(desc->page_id, desc->data);
            }
            frame_id = pool->clock_hand;
        } else {
            /* 清除引用位，继续扫描 */
            if (desc->refcount > 0) {
                desc->refcount--;
            }
        }

        pool->clock_hand = (pool->clock_hand + 1) % pool->num_pages;
    }

    return frame_id;
}
```

**对照学习点**：
- 使用 while 循环实现 Clock-Sweep 置换算法
- 循环条件是"未找到可用帧"，适合未知迭代次数
- 循环内通过条件判断控制退出

---

## 对照四：事务状态机

**源文件**：`engineering/src/db/txn/txn.c`

事务管理器使用状态机管理事务生命周期：

```c
typedef enum {
    TXN_WORKING,      /* 事务执行中 */
    TXN_PREPARING,    /* 准备提交（两阶段提交） */
    TXN_PREPARED,     /* 已准备好 */
    TXN_COMMITTING,   /* 正在提交 */
    TXN_COMMITTED,    /* 已提交 */
    TXN_ABORTING,     /* 正在回滚 */
    TXN_ABORTED       /* 已回滚 */
} TransactionState;

/* 事务状态转换 */
TransactionState txn_advance(Transaction *txn, TransactionState new_state) {
    switch (txn->state) {
        case TXN_WORKING:
            if (new_state == TXN_PREPARING) return TXN_PREPARING;
            if (new_state == TXN_ABORTING) return TXN_ABORTING;
            break;

        case TXN_PREPARING:
            if (new_state == TXN_PREPARED) return TXN_PREPARED;
            if (new_state == TXN_ABORTING) return TXN_ABORTING;
            break;

        case TXN_PREPARED:
            if (new_state == TXN_COMMITTING) return TXN_COMMITTING;
            if (new_state == TXN_ABORTING) return TXN_ABORTING;
            break;

        case TXN_COMMITTING:
            if (new_state == TXN_COMMITTED) return TXN_COMMITTED;
            break;

        case TXN_ABORTING:
            if (new_state == TXN_ABORTED) return TXN_ABORTED;
            break;

        default:
            return TXN_ERROR;  /* 无效转换 */
    }
    return TXN_ERROR;
}
```

**对照学习点**：
- 状态机控制事务的合法状态转换
- 只有合法转换才被接受，防止状态机进入非法状态
- 这是 FSM 在数据库事务管理中的典型应用

---

## 工程代码模式总结

| 组件 | 控制流模式 | 用途 |
|------|----------|------|
| SQL Parser | 状态机 | 词法分析、Token 解析 |
| WAL | goto 清理 | 资源分配与错误处理 |
| Buffer Pool | while 循环 | Clock-Sweep 置换算法 |
| Transaction Manager | 状态机 | 事务生命周期管理 |
| 网络协议 | 状态机 | 连接状态、协议解析 |

---

## 延伸阅读

- `engineering/src/db/sql/` - SQL Parser 实现
- `engineering/src/db/storage/wal/wal.c` - WAL 写日志实现
- `engineering/src/db/storage/bufmgr.c` - Buffer Pool 实现
- `engineering/src/db/txn/txn.c` - 事务管理实现
