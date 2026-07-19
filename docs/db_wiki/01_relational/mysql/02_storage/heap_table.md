# InnoDB 表空间与行格式

## 学习目标

- 理解 InnoDB 索引组织表（IOT）的核心设计
- 掌握表空间结构（System Tablespace vs File-Per-Table）
- 熟悉四种行格式（REDUNDANT/COMPACT/DYNAMIC/COMPRESSED）
- 了解 BLOB/CLOB 行外存储机制
- 对比 InnoDB IOT 与 PostgreSQL Heap Table 的设计差异

## 核心概念

- **索引组织表（IOT）**：InnoDB 表即主键索引，数据存储在 B+Tree 叶子节点
- **聚簇索引（Clustered Index）**：主键索引，叶子节点存储完整行数据
- **二级索引（Secondary Index）**：非主键索引，叶子节点存储主键值（非行指针）
- **表空间（Tablespace）**：InnoDB 存储数据的逻辑容器，由一个或多个文件组成
- **System Tablespace**：共享表空间，默认 `ibdata1`，存储元数据、undo log、doublewrite buffer
- **File-Per-Table Tablespace**：独立表空间，每表一个 `.ibd` 文件，默认启用
- **行格式（Row Format）**：行的物理存储格式，影响存储效率和查询性能
- **溢出页（Overflow Page）**：BLOB/CLOB 等大字段存储在独立的页面

## 索引组织表（IOT）架构

InnoDB 的核心设计理念是"表即索引"，数据按主键组织在 B+Tree 中。

```mermaid
graph TD
    subgraph "聚簇索引（Clustered Index）"
        ROOT[根节点<br/>内部页]
        INT1[内部节点<br/>中间页]
        INT2[内部节点<br/>中间页]

        LEAF1[叶子节点<br/>存储完整行数据]
        LEAF2[叶子节点<br/>存储完整行数据]
        LEAF3[叶子节点<br/>存储完整行数据]
    end

    ROOT --> INT1
    ROOT --> INT2
    INT1 --> LEAF1
    INT1 --> LEAF2
    INT2 --> LEAF3

    subgraph "叶子节点内容"
        ROW1[行数据: id=1, name='Alice', age=25]
        ROW2[行数据: id=2, name='Bob', age=30]
        ROW3[行数据: id=3, name='Carol', age=28]
    end

    LEAF1 --> ROW1
    LEAF2 --> ROW2
    LEAF3 --> ROW3

    style ROOT fill:#f9f,stroke:#333
    style LEAF1 fill:#9f9,stroke:#333
    style LEAF2 fill:#9f9,stroke:#333
    style LEAF3 fill:#9f9,stroke:#333
```

### 聚簇索引与二级索引的关系

```mermaid
graph TD
    subgraph "二级索引（name 列）"
        SEC_ROOT[二级索引根节点]
        SEC_LEAF[二级索引叶子节点]

        SEC_ENTRY1[索引项: name='Alice' → 主键值 1]
        SEC_ENTRY2[索引项: name='Bob' → 主键值 2]
        SEC_ENTRY3[索引项: name='Carol' → 主键值 3]
    end

    SEC_ROOT --> SEC_LEAF
    SEC_LEAF --> SEC_ENTRY1
    SEC_LEAF --> SEC_ENTRY2
    SEC_LEAF --> SEC_ENTRY3

    SEC_ENTRY1 -.回表查询.-> CLUS_LEAF1[聚簇索引叶子节点<br/>id=1 的行]
    SEC_ENTRY2 -.回表查询.-> CLUS_LEAF2[聚簇索引叶子节点<br/>id=2 的行]
    SEC_ENTRY3 -.回表查询.-> CLUS_LEAF3[聚簇索引叶子节点<br/>id=3 的行]

    style SEC_LEAF fill:#ff9,stroke:#333
    style CLUS_LEAF1 fill:#9f9,stroke:#333
```

**关键设计**：
1. **二级索引存储主键值**：不存储行指针（ctid），避免更新主键时更新所有二级索引
2. **回表查询**：二级索引查找需先找到主键值，再回聚簇索引查找完整行
3. **主键顺序存储**：数据按主键物理排序，范围查询效率高

### 无显式主键的处理

如果表没有显式主键，InnoDB 按以下顺序选择聚簇索引：

```mermaid
flowchart TD
    A[创建表] --> B{有显式主键?}
    B -->|是| C[使用显式主键作为聚簇索引]
    B -->|否| D{有唯一非空索引?}
    D -->|是| E[使用第一个唯一非空索引作为聚簇索引]
    D -->|否| F[自动生成 6 字节隐藏主键<br/>DB_ROW_ID]
```

**隐藏列说明**：
- **DB_ROW_ID**：6 字节，无主键时自动生成，单调递增
- **DB_TRX_ID**：6 字节，最后修改事务 ID
- **DB_ROLL_PTR**：7 字节，回滚指针，指向 undo log

## 表空间结构

### System Tablespace（共享表空间）

```mermaid
graph TD
    subgraph "ibdata1 文件内部结构"
        A[页面 0: FSP_HDR<br/>表空间头部]
        B[页面 1: IBUF_BITMAP<br/>Insert Buffer 位图]
        C[页面 2: INODE<br/>索引节点]
        D[页面 3: 系统表<br/>SYS_TABLES 等]

        E[Undo Log 页面]
        F[Insert Buffer 页面]
        G[Doublewrite Buffer<br/>128 个页面]
        H[用户数据页<br/>file-per-table 禁用时]
    end

    A --> B --> C --> D
    D --> E --> F --> G --> H

    style A fill:#f9f,stroke:#333
    style D fill:#ff9,stroke:#333
    style G fill:#9ff,stroke:#333
```

**存储内容**：
- **数据字典**：表、列、索引元数据（SYS_TABLES、SYS_COLUMNS、SYS_INDEXES）
- **Undo Log**：回滚日志（MySQL 8.0 可分离到独立 undo tablespace）
- **Insert Buffer**：二级索引变更缓冲
- **Doublewrite Buffer**：防止页面部分写入（128 页，2MB）
- **用户数据**：仅 `innodb_file_per_table=OFF` 时存储

### File-Per-Table Tablespace（独立表空间）

```mermaid
graph TD
    subgraph "users.ibd 文件"
        FSP[FSP_HDR 页面<br/>表空间头部]
        IBUF[IBUF_BITMAP 页面]
        INODE[INODE 页面]
        INDEX[索引页面<br/>聚簇索引 + 二级索引]

        LEAF1[叶子节点页<br/>存储行数据]
        INTERNAL[内部节点页]
        OVERFLOW[溢出页<br/>BLOB/CLOB]
    end

    FSP --> IBUF --> INODE --> INDEX
    INDEX --> LEAF1
    INDEX --> INTERNAL
    INDEX --> OVERFLOW

    style FSP fill:#f9f,stroke:#333
    style LEAF1 fill:#9f9,stroke:#333
```

**优势**：
- **独立管理**：DROP TABLE 直接删除文件，不产生碎片
- **空间回收**：TRUNCATE TABLE 瞬间完成
- **文件系统优化**：可利用文件系统特性（如 TRIM）
- **迁移方便**：可单独备份/迁移某个表

### System vs File-Per-Table 对比

```mermaid
graph LR
    subgraph "innodb_file_per_table = OFF"
        SHARED[ibdata1<br/>所有表共享]
        T1[表 A 数据]
        T2[表 B 数据]
        T3[表 C 数据]

        SHARED --> T1
        SHARED --> T2
        SHARED --> T3
    end

    subgraph "innodb_file_per_table = ON（默认）"
        F1[users.ibd<br/>表 users 独立]
        F2[orders.ibd<br/>表 orders 独立]
        F3[products.ibd<br/>表 products 独立]
        IB[ibdata1<br/>仅元数据和 undo]
    end

    style SHARED fill:#f99,stroke:#333
    style IB fill:#9f9,stroke:#333
```

**对比表**：

| 维度 | System Tablespace | File-Per-Table |
|------|------------------|----------------|
| 文件管理 | 单一大文件 | 每表独立文件 |
| DROP TABLE | 标记删除，空间不释放 | 直接删除文件，空间立即回收 |
| TRUNCATE | 逐行删除 | 瞬间重建文件 |
| 碎片整理 | 需全库导出导入 | OPTIMIZE TABLE 单表重建 |
| 表迁移 | 需导出导入 | 直接复制 .ibd 文件 |
| 适用场景 | 小型数据库 | 中大型生产环境 |

## 行格式（Row Format）

InnoDB 支持四种行格式，影响存储效率和查询性能。

### 四种行格式对比

```mermaid
graph TD
    A[行格式演变] --> B[REDUNDANT<br/>MySQL 5.0 之前]
    A --> C[COMPACT<br/>MySQL 5.0 引入]
    A --> D[DYNAMIC<br/>MySQL 5.7 默认]
    A --> E[COMPRESSED<br/>压缩格式]

    B --> F[存储效率低<br/>已废弃]
    C --> G[紧凑存储<br/>NULL 值优化]
    D --> H[大字段完全外存<br/>当前首选]
    E --> I[压缩存储<br/>节省空间但消耗 CPU]

    style D fill:#9f9,stroke:#333
    style E fill:#ff9,stroke:#333
```

### COMPACT 行格式结构

```mermaid
graph LR
    subgraph "COMPACT 行结构"
        VAR_LEN[变长字段长度列表<br/>逆序存储]
        NULL[NULL 标志位<br/>1 bit per NULLable column]
        EXTRA[记录头信息<br/>5 字节]
        COLS[列数据<br/>按定义顺序]

        DB_ROW[DB_ROW_ID<br/>6B 可选]
        DB_TRX[DB_TRX_ID<br/>6B]
        DB_ROLL[DB_ROLL_PTR<br/>7B]
        USER_COLS[用户列数据]
    end

    VAR_LEN --> NULL --> EXTRA --> COLS
    COLS --> DB_ROW --> DB_TRX --> DB_ROLL --> USER_COLS

    style VAR_LEN fill:#ff9,stroke:#333
    style NULL fill:#ff9,stroke:#333
    style EXTRA fill:#f9f,stroke:#333
```

**记录头信息（5 字节）**：

| 字段 | 位数 | 说明 |
|------|-----|------|
| `deleted_flag` | 1 | 删除标志 |
| `min_rec_flag` | 1 | B+Tree 非叶子节点最小记录标志 |
| `n_owned` | 4 | 该记录拥有的记录数（用于 Page Directory） |
| `heap_no` | 13 | 堆中位置编号 |
| `record_type` | 3 | 记录类型（000=普通，001=B+Tree 指针，010=Infimum，011=Supremum） |
| `next_record` | 16 | 下一条记录偏移 |

### DYNAMIC 行格式（默认）

```mermaid
graph TD
    subgraph "DYNAMIC 行格式处理大字段"
        A[行数据] --> B{列长度 > 768 字节?}
        B -->|是| C[该列存储在溢出页]
        B -->|否| D[存储在当前页]

        C --> E[当前页存 20 字节指针<br/>指向溢出页链表]
        E --> F[溢出页 1<br/>存储前 768 字节]
        F --> G[溢出页 2]
        G --> H[溢出页 N<br/>存储剩余数据]

        style C fill:#ff9,stroke:#333
        style E fill:#9f9,stroke:#333
    end
```

**DYNAMIC vs COMPACT**：

| 维度 | COMPACT | DYNAMIC |
|------|---------|---------|
| 大字段阈值 | > 768 字节部分外存 | > 768 字节完全外存 |
| 当前页存储 | 前 768 字节 + 指针 | 仅 20 字节指针 |
| 溢出页利用率 | 每页存储部分数据 | 每页存储完整数据 |
| 适用场景 | 传统 OLTP | 现代 OLTP + 混合负载 |

### COMPRESSED 行格式

```mermaid
graph TD
    subgraph "COMPRESSED 行格式"
        A[原始页面 16KB] --> B{压缩算法<br/>zlib 或 lz4}
        B --> C[压缩后页面<br/>可能 4KB/8KB/16KB]

        C --> D{压缩后大小?}
        D -->|≤ 4KB| E[存储为 4KB 页面]
        D -->|≤ 8KB| F[存储为 8KB 页面]
        D -->|> 8KB| G[存储为 16KB 页面<br/>压缩失败]

        style E fill:#9f9,stroke:#333
        style F fill:#9f9,stroke:#333
    end
```

**压缩配置**：
```sql
CREATE TABLE compressed_table (
    id INT PRIMARY KEY,
    data TEXT
) ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8;
-- KEY_BLOCK_SIZE 可选: 1/2/4/8（对应压缩后页大小）
```

**压缩的利弊**：

| 优点 | 缺点 |
|------|------|
| 节省磁盘空间 50-70% | CPU 消耗增加 |
| 减少 I/O 读取量 | 压缩/解压缩延迟 |
| 增加 Buffer Pool 有效容量 | 压缩失败浪费空间 |
| 适合读多写少 | 不适合频繁更新 |

## 溢出页（Overflow Page）

当行数据超过页面大小的一半（约 8KB）时，InnoDB 使用溢出页存储大字段。

### 溢出页链表结构

```mermaid
graph TD
    subgraph "主页面"
        MAIN[聚簇索引页面]
        PTR[20 字节指针<br/>指向溢出页链表]
    end

    subgraph "溢出页链表"
        OVF1[溢出页 1<br/>768 字节数据]
        OVF2[溢出页 2<br/>768 字节数据]
        OVF3[溢出页 3<br/>剩余数据]
    end

    MAIN --> PTR
    PTR --> OVF1
    OVF1 --> OVF2
    OVF2 --> OVF3

    style PTR fill:#ff9,stroke:#333
    style OVF1 fill:#9f9,stroke:#333
```

**溢出页指针结构（20 字节）**：

| 字段 | 大小 | 说明 |
|------|-----|------|
| space_id | 4B | 表空间 ID |
| page_no | 4B | 溢出页号 |
| offset | 4B | 数据在页内偏移 |
| length | 8B | 外存数据长度 |

### BLOB/CLOB 存储策略

```mermaid
flowchart TD
    A[BLOB/TEXT 列] --> B{列长度?}

    B -->|= 0| C[存储 NULL<br/>仅 NULL 标志位]
    B -->|≤ 40 字节| D[存储在当前页<br/>inline 存储]
    B -->|40-768 字节| E[前 40 字节在当前页<br/>剩余在溢出页]
    B -->|> 768 字节| F[DYNAMIC: 完全外存<br/>COMPACT: 前 768 + 外存]

    style C fill:#f9f,stroke:#333
    style D fill:#9f9,stroke:#333
    style F fill:#ff9,stroke:#333
```

**注意事项**：
1. **查询性能**：读取大字段需要额外 I/O，可能成为性能瓶颈
2. **更新代价**：更新大字段可能触发溢出页重新分配
3. **Buffer Pool 污染**：大字段可能占用大量缓存，建议分离存储

## 行大小限制

InnoDB 行最大长度为 65535 字节（MySQL 限制），但实际受页面大小限制。

### 行大小计算规则

```mermaid
flowchart TD
    A[计算行大小] --> B[变长字段长度列表]
    B --> C[NULL 标志位]
    C --> D[记录头 5 字节]
    D --> E[隐藏列 DB_TRX_ID + DB_ROLL_PTR = 13 字节]
    E --> F[用户列数据]

    F --> G{总长度 > 页面一半?}
    G -->|是| H[触发溢出页机制]
    G -->|否| I[正常存储在当前页]

    style H fill:#ff9,stroke:#333
    style I fill:#9f9,stroke:#333
```

**示例计算**：

假设表定义：
```sql
CREATE TABLE example (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    description TEXT,
    created_at TIMESTAMP
);
```

行大小计算：
- 变长字段长度列表：2 字节（name + description）
- NULL 标志位：1 字节（3 个 NULLable 列，向上取整到 8 bit）
- 记录头：5 字节
- 隐藏列：13 字节
- id：4 字节
- name：最大 100 字节
- description：TEXT 列，超过阈值外存
- created_at：4 字节

**实际存储大小** ≈ 2 + 1 + 5 + 13 + 4 + 100 + 4 = 129 字节（不含 TEXT 外存）

## 页面组织与 B+Tree 结构

### B+Tree 叶子节点链表

```mermaid
graph LR
    subgraph "聚簇索引 B+Tree 叶子节点"
        PAGE1[页面 1<br/>id 1-100]
        PAGE2[页面 2<br/>id 101-200]
        PAGE3[页面 3<br/>id 201-300]
        PAGE4[页面 4<br/>id 301-400]
    end

    PAGE1 -->|next| PAGE2
    PAGE2 -->|next| PAGE3
    PAGE3 -->|next| PAGE4

    PAGE1 -.prev.-> NULL
    PAGE4 -.next.-> NULL

    style PAGE1 fill:#9f9,stroke:#333
    style PAGE2 fill:#9f9,stroke:#333
    style PAGE3 fill:#9f9,stroke:#333
    style PAGE4 fill:#9f9,stroke:#333
```

**页面结构**：
- **File Header**：38 字节，包含页面号、前后指针、LSN、页面类型等
- **Page Header**：56 字节，包含记录数、槽位数、空闲空间等
- **Infimum + Supremum**：系统记录，标记最小/最大值
- **User Records**：用户数据区
- **Free Space**：空闲空间
- **Page Directory**：页目录，加速查找
- **File Trailer**：8 字节，校验和 + LSN

### 页面内部记录组织

```mermaid
graph TD
    subgraph "页面内部结构"
        FH[File Header<br/>38B]
        PH[Page Header<br/>56B]
        INF[Infimum<br/>系统最小记录]
        REC[User Records<br/>用户记录链表]
        SUP[Supremum<br/>系统最大记录]
        FREE[Free Space]
        PD[Page Directory<br/>槽位数组]
        FT[File Trailer<br/>8B]
    end

    FH --> PH --> INF --> REC --> SUP --> FREE --> PD --> FT

    INF -.next.-> R1[记录 1]
    R1 -.next.-> R2[记录 2]
    R2 -.next.-> R3[记录 3]
    R3 -.next.-> SUP

    style INF fill:#f9f,stroke:#333
    style SUP fill:#f9f,stroke:#333
    style REC fill:#9f9,stroke:#333
```

**记录链表**：
- 所有记录按主键顺序组成单向链表
- Infimum 指向第一条用户记录
- 最后一条用户记录指向 Supremum
- Page Directory 提供二分查找加速

## 与 PostgreSQL Heap Table 的对比

```mermaid
graph TD
    subgraph "InnoDB IOT"
        A1[表即聚簇索引]
        A2[数据按主键排序]
        A3[二级索引存主键值]
        A4[更新主键代价大]
        A5[范围查询高效]
    end

    subgraph "PostgreSQL Heap"
        B1[表是独立堆结构]
        B2[数据按插入顺序]
        B3[索引存 ctid 行指针]
        B4[更新主键代价小]
        B5[随机访问高效]
    end

    A1 -.对比.-> B1
    A2 -.对比.-> B2
    A3 -.对比.-> B3
    A4 -.对比.-> B4
    A5 -.对比.-> B5
```

### 详细对比表

| 维度 | InnoDB IOT | PostgreSQL Heap |
|------|-----------|----------------|
| 数据组织 | 主键聚簇索引 | 无序堆 |
| 主键要求 | 强烈推荐 | 可选 |
| 二级索引内容 | 主键值 | ctid（行指针） |
| 更新主键代价 | 高（需移动行 + 更新所有二级索引） | 低（仅更新索引） |
| 范围查询性能 | 高（顺序 I/O） | 中（随机 I/O） |
| 插入性能 | 高（主键顺序） | 高（堆尾部追加） |
| 空间利用率 | 中（索引开销） | 高（无索引组织开销） |
| 全表扫描 | 中（按主键顺序） | 高（按 ctid 顺序） |
| MVCC 实现 | Undo Log + Read View | xmin/xmax 多版本 |

### 设计哲学差异

**InnoDB IOT**：
- 假设：主键是核心访问路径，范围查询常见
- 策略：数据按主键组织，牺牲更新主键的性能
- 优势：范围查询、主键点查高效
- 劣势：更新主键代价大，无主键表性能差

**PostgreSQL Heap**：
- 假设：访问模式多样，更新频繁
- 策略：堆存储 + 独立索引，更新主键无额外开销
- 优势：更新主键高效，全表扫描友好
- 劣势：范围查询可能随机 I/O

## 配置最佳实践

### 表空间配置

```sql
-- 查看当前表空间配置
SHOW VARIABLES LIKE 'innodb_file_per_table';

-- 启用独立表空间（MySQL 5.6+ 默认）
SET GLOBAL innodb_file_per_table = ON;

-- 查看表空间文件
SELECT 
    TABLE_NAME,
    ENGINE,
    TABLESPACE_NAME,
    FILE_NAME
FROM information_schema.INNODB_TABLES
JOIN information_schema.INNODB_TABLESPACES 
    USING (SPACE);
```

### 行格式选择

| 场景 | 推荐行格式 | 说明 |
|------|----------|------|
| OLTP + 小字段 | DYNAMIC | 默认，综合最优 |
| OLTP + 大字段多 | DYNAMIC | 完全外存，保护 Buffer Pool |
| 读多写少 | COMPRESSED | 节省空间，减少 I/O |
| 写多读少 | DYNAMIC | 避免压缩开销 |
| 兼容旧版本 | COMPACT | MySQL 5.0+ 兼容 |

### 大字段处理建议

```sql
-- 分离存储大字段
CREATE TABLE articles (
    id INT PRIMARY KEY,
    title VARCHAR(200),
    author VARCHAR(100)
);

CREATE TABLE article_contents (
    article_id INT PRIMARY KEY,
    content LONGTEXT,
    FOREIGN KEY (article_id) REFERENCES articles(id)
) ROW_FORMAT=DYNAMIC;

-- 或使用压缩
CREATE TABLE compressed_blob (
    id INT PRIMARY KEY,
    data LONGBLOB
) ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8;
```

## 要点总结

- InnoDB 是 **索引组织表（IOT）**，数据存储在聚簇索引叶子节点，按主键排序
- 二级索引存储 **主键值** 而非行指针，回表查询需先找主键值
- **File-Per-Table Tablespace** 是默认推荐方案，便于空间回收和表迁移
- 四种行格式中 **DYNAMIC** 是现代首选，大字段完全外存保护 Buffer Pool
- **溢出页链表** 存储超过页面一半的大字段，可能成为性能瓶颈
- 与 PostgreSQL Heap 相比，InnoDB IOT 对 **范围查询和主键点查** 更友好
- 更新主键代价大，建议使用 **自增主键或雪花算法主键**

## 思考题

1. 为什么 InnoDB 选择索引组织表而非堆表？这种设计在什么场景下优势最明显？

2. 二级索引存储主键值而非行指针，这种设计的利弊是什么？如果表有多个二级索引，更新主键时会发生什么？

3. 如果一个表没有主键，InnoDB 会如何处理？这种情况下性能如何？

4. DYNAMIC 和 COMPRESSED 行格式在存储大字段时有何不同？在什么场景下应该选择 COMPRESSED？

5. 为什么不建议频繁更新主键？如果必须更新，有什么优化策略？

6. 比较 InnoDB IOT 和 PostgreSQL Heap 在 MVCC 实现上的差异，这对并发性能有什么影响？
