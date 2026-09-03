# 存储引擎架构图

> 本文档记录数据库存储引擎的完整架构设计，参考 PostgreSQL 风格实现。

---

## 1. 整体架构

```mermaid
graph TB
    subgraph "SQL 执行层"
        SQL_PARSER["SQL 解析器<br/>(Parser)"]
        SQL_OPT["查询优化器<br/>(Optimizer)"]
        SQL_EXEC["SQL 执行器<br/>(Executor)"]
    end

    subgraph "访问方法层 (Access Method)"
        REL["Relation 抽象层"]
        HEAP["Heap AM<br/>(堆表)"]
        BTREE["BTree AM<br/>(BTree 索引)"]
        HASH["Hash AM<br/>(Hash 索引)"]
    end

    subgraph "缓冲层 (Buffer Pool)"
        BUFMGR["Buffer Manager"]
        BUFTABLE["Buffer Hash 表"]
        CLOCK["Clock-Sweep 置换器"]
    end

    subgraph "日志层 (WAL)"
        WAL_LOG["WAL 日志"]
        WAL_BUF["WAL Buffer"]
        CHECKPOINT["检查点"]
    end

    subgraph "并发控制层 (MVCC)"
        SNAPSHOT["快照管理"]
        VERSION["版本链"]
        UNDO["Undo 日志"]
        GC["垃圾回收"]
    end

    subgraph "目录层 (Catalog)"
        CAT["系统目录<br/>(pg_class, pg_attribute)"]
        OID["OID 分配器"]
    end

    subgraph "磁盘层"
        DISK["磁盘文件管理"]
        PAGE["页面管理"]
    end

    SQL_PARSER --> SQL_OPT
    SQL_OPT --> SQL_EXEC
    SQL_EXEC --> REL

    REL --> HEAP
    REL --> BTREE
    REL --> HASH

    HEAP --> BUFMGR
    BTREE --> BUFMGR
    HASH --> BUFMGR

    BUFMGR <--> BUFTABLE
    BUFMGR <--> CLOCK
    BUFMGR --> WAL_LOG

    WAL_LOG --> WAL_BUF
    WAL_BUF --> CHECKPOINT

    HEAP --> SNAPSHOT
    SNAPSHOT --> VERSION
    VERSION --> UNDO
    UNDO --> GC

    SQL_EXEC --> CAT
    CAT --> OID

    BUFMGR --> DISK
    DISK --> PAGE
```

---

## 2. Buffer Pool 架构

```mermaid
graph TB
    subgraph "Buffer Pool Manager"
        BP_HEADER["Buffer Pool Header<br/>(N buffers)"]
    end

    subgraph "Buffer 描述符数组"
        BD0["BufferDesc[0]"]
        BD1["BufferDesc[1]"]
        BD2["BufferDesc[...]"]
        BDN["BufferDesc[N-1]"]
    end

    subgraph "数据页内存"
        PAGE0["Page 0"]
        PAGE1["Page 1"]
        PAGEN["Page N-1"]
    end

    subgraph "Hash 表 (快速查找)"
        HASH_TABLE["Hash 表<br/>(relfilenode + blocknum → buf_id)"]
    end

    subgraph "Clock-Sweep 置换"
        CLOCK["Clock Hand"]
    end

    BP_HEADER --> BD0
    BP_HEADER --> BD1
    BP_HEADER --> BD2
    BP_HEADER --> BDN

    BD0 --> PAGE0
    BD1 --> PAGE1
    BDN --> PAGEN

    HASH_TABLE -.->|查找| BD0
    HASH_TABLE -.->|查找| BD1

    CLOCK -.->|置换| BD2

    subgraph "BufferDesc 结构"
        subgraph "字段"
            F1["buf_id: Buffer ID"]
            F2["state: 状态标志"]
            F3["relfilenode: 文件节点"]
            F4["blocknum: 块号"]
            F5["usage_count: Clock 计数"]
            F6["refcount: Pin 计数"]
            F7["hash_next/hash_prev: 链表"]
        end
    end

    style HASH_TABLE fill:#e1f5fe
    style CLOCK fill:#fff3e0
```

**关键概念：**

| 概念 | 说明 |
|------|------|
| **BufferDesc** | 每个 Buffer 的元数据描述符，包含状态、锁信息、Hash 链表指针 |
| **Hash 表** | 通过 (relfilenode, blocknum) 快速定位 Buffer |
| **Clock-Sweep** | 页面置换算法，遍历环形缓冲区找 usage_count=0 的页面 |
| **Pin/Unpin** | Pin 增加 refcount 表示页面正在使用，不能被置换 |
| **Dirty Page** | 标记为脏的页面需要在置换前刷盘 |

---

## 3. BTree 索引架构

```mermaid
graph TB
    subgraph "BTree 结构"
        ROOT["根页面<br/>(Root, level=N)"]
        
        INTL1["内部页面 1<br/>(level=N-1)"]
        INTL2["内部页面 2<br/>(level=N-1)"]
        
        LEAF1["叶子页面 1<br/>(level=0)"]
        LEAF2["叶子页面 2<br/>(level=0)"]
        LEAF3["叶子页面 3<br/>(level=0)"]
        LEAF4["叶子页面 4<br/>(level=0)"]
    end

    ROOT --> INTL1
    ROOT --> INTL2

    INTL1 --> LEAF1
    INTL1 --> LEAF2
    INTL2 --> LEAF3
    INTL2 --> LEAF4

    subgraph "叶子页面结构"
        LP1_1["LinePointer[0] → Tuple1"]
        LP1_2["LinePointer[1] → Tuple2"]
        LP1_3["LinePointer[...] → [...]"]
    end

    subgraph "内部页面结构"
        ITM1["Item: key1, downlink → child"]
        ITM2["Item: key2, downlink → child"]
    end

    LEAF1 --- LP1_1
    LEAF1 --- LP1_2
    LEAF1 --- LP1_3

    INTL1 --- ITM1
    INTL1 --- ITM2

    subgraph "页面布局"
        PAGE_HDR["PageHeader<br/>20 bytes"]
        ITEMS["Items (反向生长)"]
        FREE["Free Space"]
        TUPLES["Tuples (正向生长)"]
    end

    PAGE_HDR --> ITEMS
    ITEMS -.->|反向| TUPLES
    FREE -.->|空闲| ITEMS
    TUPLES -.->|正向| FREE

    style ROOT fill:#c8e6c9
    style LEAF1 fill:#bbdefb
    style LEAF2 fill:#bbdefb
    style LEAF3 fill:#bbdefb
    style LEAF4 fill:#bbdefb
```

**BTree 查找过程：**

```
1. 从根页面开始
2. 在内部页面二分查找：key < downlink_key → 左子树
3. 重复直到叶子页面
4. 在叶子页面二分查找匹配的键
5. 返回 heap tuple 指针
```

**页面分裂（Insert 时）：**

```
当页面满时：
1. 分配新页面
2. 将后半部分数据移动到新页面
3. 在父页面插入新的 downlink
4. 如果父页面也满，继续分裂向上传播
```

---

## 4. WAL (Write-Ahead Logging) 架构

```mermaid
graph TB
    subgraph "WAL 文件结构"
        WAL_HDR["WAL Header<br/>(64 bytes)<br/>- magic: WAL1<br/>- version<br/>- page_size<br/>- checksum"]
        
        subgraph "Log Records (循环追加)"
            REC1["Record 1<br/>LSN=100<br/>BEGIN"]
            REC2["Record 2<br/>LSN=200<br/>INSERT"]
            REC3["Record 3<br/>LSN=300<br/>UPDATE"]
            REC4["Record N<br/>LSN=N<br/>CHECKPOINT"]
        end
    end

    subgraph "Record 结构 (24 bytes header)"
        RH_TYPE["type: 1 byte"]
        RH_SIZE["size: 3 bytes"]
        RH_LSN["lsn: 8 bytes"]
        RH_TXN["txn_id: 4 bytes"]
        RH_PREV["prev_lsn: 4 bytes"]
        RH_CKSUM["checksum: 4 bytes"]
    end

    subgraph "日志类型"
        LOG_TYPES["BEGIN | INSERT | UPDATE | DELETE | COMMIT | ABORT | CHECKPOINT"]
    end

    WAL_HDR --> REC1
    REC1 --> REC2
    REC2 --> REC3
    REC3 --> REC4

    LOG_TYPES --> RH_TYPE

    style WAL_HDR fill:#fff9c4
    style REC1 fill:#e1f5fe
    style REC2 fill:#e1f5fe
    style REC3 fill:#e1f5fe
```

**WAL 与 Buffer Pool 的协作：**

```mermaid
sequenceDiagram
    participant Client as 客户端
    participant Exec as 执行器
    participant WAL as WAL
    participant BP as Buffer Pool
    participant Disk as 磁盘

    Client->>Exec: UPDATE t SET col=val WHERE id=1

    Exec->>BP: buf_read(relfilenode, blocknum)
    BP-->>Exec: BufferDesc

    Exec->>Exec: 修改内存中的数据

    Exec->>WAL: wal_write_update(key, old_val, new_val)
    WAL-->>Exec: LSN=300

    Exec->>BP: buf_dirty(buffer)
    Note over BP: 设置 BM_DIRTY 标志

    Exec->>WAL: wal_write_commit(txn_id)
    WAL-->>Exec: LSN=350
    WAL->>Disk: 刷日志 (fsync)

    Note over BP: 延迟刷盘，批量写入

    Exec-->>Client: 事务提交成功
```

**检查点 (Checkpoint) 流程：**

```
1. 写入 CHECKPOINT 日志记录
2. 将所有脏页刷到磁盘
3. 更新控制文件中的检查点位置
4. 后续恢复只需从检查点开始
```

---

## 5. MVCC (多版本并发控制) 架构

```mermaid
graph TB
    subgraph "事务系统"
        TXN1["事务 T1<br/>(txn_id=100)"]
        TXN2["事务 T2<br/>(txn_id=101)"]
        TXN3["事务 T3<br/>(txn_id=102)"]
    end

    subgraph "快照 (Snapshot)"
        SNAP["ReadView<br/>xmin=99<br/>xmax=103<br/>xip_list=[100,101]"]
    end

    subgraph "版本链"
        V3["Version 3<br/>xmin=102, xmax=0<br/>data='C'"]
        V2["Version 2<br/>xmin=101, xmax=102<br/>data='B'"]
        V1["Version 1<br/>xmin=100, xmax=0<br/>data='A'"]
    end

    TXN1 --> SNAP
    TXN2 --> SNAP
    TXN3 --> V3

    V3 --> V2
    V2 --> V1

    subgraph "可见性判断"
        VIS["mvcc_version_visible()"]
    end

    SNAP --> VIS
    V1 --> VIS
    V2 --> VIS
    V3 --> VIS

    subgraph "Undo 日志 (用于回滚和 GC)"
        U1["Undo: INSERT → DELETE"]
        U2["Undo: UPDATE B → A"]
    end

    V1 -.-> U1
    V2 -.-> U2
```

**快照可见性规则：**

```
版本 V 对快照 S 可见当且仅当：

1. xmin 规则：V.xmin 在快照中已提交
   - V.xmin < S.xmax
   - V.xmin 不在 S.xip_list 中

2. xmax 规则：V 未被已提交的事务删除
   - V.xmax = 0（从未被删除）
   - 或 V.xmax 在快照中仍活跃（未提交）
   - 或 V.xmax >= S.xmax（删除事务未提交）

3. 自可见：事务自身创建的版本始终可见
   - V.xmin == 当前事务ID
```

**版本链遍历：**

```c
mvcc_version* mvcc_version_find_visible(
    mvcc_version_t* head,      // 版本链头
    mvcc_snapshot_t* snapshot,  // 快照
    mvcc_txn_id_t cur_txn_id   // 当前事务ID
) {
    for (v = head; v != NULL; v = v->next) {
        if (mvcc_version_visible(snapshot, v->xmin, v->xmax, cur_txn_id)) {
            return v;  // 找到可见版本
        }
    }
    return NULL;  // 无可见版本
}
```

---

## 6. Heap 存储架构

```mermaid
graph TB
    subgraph "堆表页面结构"
        PAGE_HDR["PageHeaderData<br/>24 bytes<br/>- pd_lsn<br/>- pd_lower<br/>- pd_upper<br/>- pd_special"]
        
        subgraph "LinePointer 数组 (pd_lower 起始)"
            LP1["[0] offset=128, flags=USED"]
            LP2["[1] offset=256, flags=USED"]
            LP3["[2] offset=0, flags=DEAD"]
            LP4["[3] offset=384, flags=USED"]
        end
        
        subgraph "Tuple 数据 (pd_upper 起始，向低地址生长)"
            T1["Tuple 1: xmin=100, xmax=0, data"]
            T2["Tuple 2: xmin=101, xmax=102, data"]
            T3["Tuple 3: xmin=103, xmax=0, data"]
        end
        
        subgraph "空闲空间 (pd_lower 到 pd_upper 之间)"
            FREE["Free Space"]
        end
    end

    PAGE_HDR --> LP1
    PAGE_HDR --> LP2
    PAGE_HDR --> LP3
    PAGE_HDR --> LP4

    T1 -.->|在页面中| LP1
    T2 -.->|在页面中| LP2
    T3 -.->|在页面中| LP4

    LP3 -.x.|已删除| T2

    subgraph "MVCC 字段"
        T_XMIN["t_xmin: 创建事务ID"]
        T_XMAX["t_xmax: 删除事务ID"]
        T_CID["t_cid: 命令ID"]
        T_CTID["t_ctid: 版本链指针"]
    end

    T1 --> T_XMIN
    T1 --> T_XMAX
    T1 --> T_CID
    T1 --> T_CTID

    style FREE fill:#f5f5f5
    style PAGE_HDR fill:#e8f5e9
```

**插入流程：**

```
heap_insert(rel, tuple):
1. 获取目标页面 Buffer
2. 检查页面空闲空间
3. 如果空间不足，分配新页面
4. 在 pd_upper 位置写入 Tuple
5. 在 pd_lower 位置添加 LinePointer
6. 更新 pd_lower 和 pd_upper
7. 更新 pd_lsn
8. 标记 Buffer 为脏
```

**HOT (Heap-Only Tuple) 更新：**

```
UPDATE 时尽量在同一页面内完成：
1. 在页面内添加新 Tuple
2. 旧 Tuple 的 t_xmax = 当前事务ID
3. 旧 Tuple 的 t_ctid → 新 Tuple
4. 避免更新所有索引（只更新必要索引）

好处：
- 减少随机 I/O
- 减少索引维护开销
```

---

## 7. 模块依赖关系

```mermaid
graph LR
    subgraph "依赖层次 (从上到下)"
        SQL["SQL 层"]
        AM["Access Method"]
        MVCC["MVCC"]
        BP["Buffer Pool"]
        CAT["Catalog"]
        WAL["WAL"]
        DISK["磁盘"]
    end

    SQL --> AM
    SQL --> CAT
    SQL --> MVCC

    AM --> MVCC
    AM --> BP

    MVCC --> BP
    MVCC --> WAL

    BP --> WAL
    BP --> DISK

    CAT --> BP
    WAL --> DISK
```

---

## 8. 数据流总览

```mermaid
flowchart TB
    subgraph "INSERT 数据流"
        I1["解析 SQL"]
        I2["优化查询计划"]
        I3["获取表锁/行锁"]
        I4["分配事务ID"]
        I5["创建新版本"]
        I6["写入 WAL"]
        I7["修改 Buffer"]
        I8["标记脏页"]
        I9["提交事务"]
        I10["刷 WAL (fsync)"]
    end

    I1 --> I2 --> I3 --> I4 --> I5 --> I6 --> I7 --> I8 --> I9 --> I10

    subgraph "SELECT 数据流"
        S1["解析 SQL"]
        S2["创建快照"]
        S3["获取 MVCC 快照"]
        S4["扫描 Heap"]
        S5["遍历版本链"]
        S6["可见性判断"]
        S7["返回可见版本"]
    end

    S1 --> S2 --> S3 --> S4 --> S5 --> S6 --> S7

    subgraph "恢复流程"
        R1["读取检查点"]
        R2["分析 WAL"]
        R3["确定活动事务"]
        R4["重做已提交"]
        R5["撤销未提交"]
        R6["恢复完成"]
    end

    R1 --> R2 --> R3 --> R4 --> R5 --> R6
```

---

## 9. 核心数据结构速查

| 模块 | 核心结构 | 用途 |
|------|----------|------|
| Buffer Pool | `BufferDesc_s` | Buffer 元数据，包含状态、Pin 计数、Hash 链表指针 |
| BTree | `BTPageHeaderData` | BTree 页面头，包含层级、指针链 |
| BTree | `BTreekirtData` | 索引元组，指向堆元组 |
| Heap | `PageHeaderData` | 堆页面头，包含 LSN、空间管理 |
| Heap | `HeapLinePointerData` | 行指针，指向页面中的元组 |
| WAL | `wal_record_header` | 日志记录头，24 字节 |
| MVCC | `mvcc_snapshot` | 快照，包含 xmin/xmax/xip_list |
| MVCC | `mvcc_version` | 行版本，包含 xmin/xmax/版本链指针 |
| MVCC | `mvcc_undo_record` | Undo 记录，用于回滚和 GC |

---

*文档版本: v1.0*
*最后更新: 2026-07-12*
