# B+Tree 索引

## 学习目标

- 理解 InnoDB B+Tree 索引的完整结构，包括聚簇索引和二级索引的差异
- 掌握聚簇索引的物理存储方式（叶子节点存储完整行数据）
- 理解二级索引的回表过程和覆盖索引的优化原理
- 熟悉 B+Tree 的 Page Split 和 Page Merge 机制
- 掌握索引组织表（IOT）与 Heap 表的本质区别

## 核心概念

- **聚簇索引（Clustered Index）**：主键就是聚簇索引，叶子节点存储完整行数据
- **二级索引（Secondary Index）**：叶子节点存储索引列值 + 主键值，需要回表
- **覆盖索引（Covering Index）**：索引包含查询所需的所有列，无需回表
- **Page Split**：插入导致页面满时分裂为两个页面
- **Page Merge**：删除导致页面利用率过低时与兄弟页面合并
- **索引组织表（Index Organized Table）**：InnoDB 的表就是聚簇索引，数据按主键排序

## 聚簇索引 B+Tree 结构

InnoDB 使用 B+Tree 作为索引结构，**数据即索引**——表数据存储在聚簇索引的叶子节点中。

```mermaid
graph TD
    subgraph "B+Tree 层级结构"
        level1[根节点<br/>Page No=3<br/>高度 3]
        level2a[内部节点<br/>Page No=5<br/>高度 2]
        level2b[内部节点<br/>Page No=8<br/>高度 2]
        leaf1[叶子节点<br/>Page No=10<br/>完整行数据]
        leaf2[叶子节点<br/>Page No=11<br/>完整行数据]
        leaf3[叶子节点<br/>Page No=12<br/>完整行数据]
        leaf4[叶子节点<br/>Page No=13<br/>完整行数据]
        leaf5[叶子节点<br/>Page No=14<br/>完整行数据]
    end

    level1 --> level2a
    level1 --> level2b
    level2a --> leaf1
    level2a --> leaf2
    level2a --> leaf3
    level2b --> leaf4
    level2b --> leaf5

    leaf1 -.->|next 指针| leaf2
    leaf2 -.->|next 指针| leaf3
    leaf3 -.->|next 指针| leaf4
    leaf4 -.->|next 指针| leaf5
    leaf5 -.->|prev 指针| leaf4
    leaf4 -.->|prev 指针| leaf3
    leaf3 -.->|prev 指针| leaf2
    leaf2 -.->|prev 指针| leaf1

    subgraph "叶子节点内部结构"
        page_structure[Page Header<br/>Page Body<br/>Infimum / Supremum<br/>User Records<br/>Page Directory]
    end

    leaf1 --> page_structure
```

### 叶子节点页面结构

```mermaid
graph LR
    A[InnoDB Page<br/>默认 16KB] --> B[Page Header 38B<br/>FIL_PAGE_OFFSET<br/>PAGE_LEVEL<br/>PAGE_N_RECS]
    A --> C[Infimum + Supremum<br/>边界记录]
    A --> D[User Records<br/>实际行数据]
    A --> E[Free Space<br/>空闲空间]
    A --> F[Page Directory<br/>行指针数组<br/>二分查找]
    A --> G[Page Trailer 8B<br/>FIL_PAGE_END_LSN<br/>校验和]

    D --> D1[Row 1: 主键=1, name='Alice', age=25]
    D --> D2[Row 2: 主键=2, name='Bob', age=30]
    D --> D3[Row 3: 主键=3, name='Charlie', age=35]
    D --> D4[...更多行]
```

### 聚簇索引的存储特点

1. **数据即索引**：表数据就是聚簇索引的叶子节点，不需要独立的 Heap 区域
2. **按主键排序**：行数据按主键顺序存储在叶子节点中，形成双向链表
3. **主键查找快**：B+Tree 查找主键值，一次定位到行数据
4. **插入顺序影响**：按主键顺序插入最高效，随机插入可能导致频繁 Page Split

```sql
-- 主键查找：B+Tree 路径
SELECT * FROM users WHERE id = 100;
-- 1. 从根节点开始
-- 2. 比较 100 与内部节点中的分隔键
-- 3. 进入对应的子节点
-- 4. 到达叶子节点，读取行数据
-- 典型高度: 2-4 层
```

## 没有主键时 InnoDB 的处理

```mermaid
flowchart TD
    A[InnoDB 创建表] --> B{指定了主键?}
    B -->|是| C[使用指定主键<br/>作为聚簇索引]
    B -->|否| D{有非空唯一索引?}
    D -->|是| E[选择第一个非空<br/>唯一索引作为聚簇索引]
    D -->|否| F[自动生成 6 字节<br/>ROWID 作为隐式主键]
    
    F --> G[ROWID 单调递增<br/>用户不可见<br/>不可用于查询]
```

## 二级索引的回表过程

二级索引的叶子节点存储的是**索引列值 + 主键值**，而不是行数据的直接指针。查询时需要先通过二级索引找到主键，再通过主键回表查找聚簇索引。

```mermaid
graph TD
    subgraph "二级索引查找"
        A1[二级索引 idx_name<br/>叶子节点: name + id]
        A1 --> B1[查找 name='Alice'<br/>返回主键 id=100]
    end

    subgraph "回表聚簇索引"
        B1 --> C1[通过主键 id=100<br/>查找聚簇索引]
        C1 --> D1[聚簇索引叶子节点<br/>读取完整行数据]
        D1 --> E1[返回所有列]
    end

    subgraph "示例数据"
        idx_page[二级索引叶子<br/>Alice, 100<br/>Bob, 200<br/>Charlie, 300]
        clust_page[聚簇索引叶子<br/>id=100, name=Alice, age=25, email=alice@...<br/>id=200, name=Bob, age=30, email=bob@...]
    end

    A1 --> idx_page
    C1 --> clust_page
```

### 回表路径分解

```sql
-- 查询: 通过 name 索引查找用户
SELECT * FROM users WHERE name = 'Alice';
-- 假设: 主键 id, 二级索引 idx_name(name)

-- 步骤 1: 二级索引查找
-- idx_name 的 B+Tree 查找 'Alice'
-- 叶子节点返回: ('Alice', 100)

-- 步骤 2: 回表
-- 通过主键 100 查找聚簇索引
-- 聚簇索引 B+Tree 查找 100
-- 叶子节点返回完整行: (100, 'Alice', 25, 'alice@example.com')

-- 步骤 3: 返回结果
-- id=100, name='Alice', age=25, email='alice@example.com'
```

### 二级索引回表的代价

```mermaid
graph LR
    A[回表代价] --> B[一次二级索引 B+Tree 查找<br/>2-4 层]
    A --> C[一次聚簇索引 B+Tree 查找<br/>2-4 层]
    A --> D[合计: 4-8 次页面随机 IO]

    E[影响回表代价的因素] --> F[索引选择性<br/>选择性高 → 回表少]
    E --> G[Buffer Pool 缓存<br/>热点页面命中]
    E --> H[覆盖索引<br/>避免回表]
```

## 覆盖索引（Covering Index）

当索引包含查询所需的所有列时，无需回表，这就是覆盖索引。

### 覆盖索引 vs 非覆盖索引的查询路径对比

```mermaid
graph TD
    subgraph "非覆盖索引（需要回表）"
        A1[查询: SELECT *<br/>FROM users<br/>WHERE name = 'Alice']
        A1 --> B1[idx_name 二级索引<br/>扫描并找到主键]
        B1 --> C1[回表聚簇索引<br/>读取完整行]
        C1 --> D1[返回所有列]
    end

    subgraph "覆盖索引（无需回表）"
        A2[查询: SELECT id, name<br/>FROM users<br/>WHERE name = 'Alice']
        A2 --> B2[idx_name 二级索引<br/>扫描并找到数据]
        B2 --> C2[直接返回 id, name<br/>无需回表！]
    end

    subgraph "性能差异"
        E1[非覆盖索引: 2 次 B+Tree 查找<br/>4-8 个页面 IO]
        E2[覆盖索引: 1 次 B+Tree 查找<br/>2-4 个页面 IO]
    end

    C1 --> E1
    C2 --> E2
```

### 覆盖索引设计

```sql
-- 常见查询模式
SELECT id, name, status FROM users WHERE status = 'active';

-- 创建覆盖索引
CREATE INDEX idx_status_covering ON users(status, name, id);
-- 或更简洁的
CREATE INDEX idx_status_covering ON users(status, name);

-- 查询时 Extra 显示: Using index（表示覆盖索引）
EXPLAIN SELECT id, name FROM users WHERE status = 'active'\G
-- Extra: Using index
```

## B+Tree 结构细节

### 非叶子节点结构

非叶子节点只存储**键值 + 子节点 Page No**，不存储行数据，因此一个页面可以容纳大量键值，使 B+Tree 保持低高度。

```mermaid
graph TD
    A[内部节点 Page<br/>16KB] --> B[键值 1 → 子节点 Page No=5]
    A --> C[键值 2 → 子节点 Page No=8]
    A --> D[键值 3 → 子节点 Page No=12]
    A --> E[键值 4 → 子节点 Page No=15]
    A --> F[...更多键值对]

    G[一个 16KB 页面<br/>可存储约 1000 个键值对] --> H[B+Tree 高度公式]
    H --> I[高度 = log_1000(行数)]

    J[行数估算] --> K[100 万行: 高度 = 2<br/>1000 万行: 高度 = 3<br/>10 亿行: 高度 = 4]
```

### 叶子节点双向链表

```mermaid
graph LR
    A[叶子节点 10<br/>主键 1-100] -->|next| B[叶子节点 11<br/>主键 101-200]
    B -->|next| C[叶子节点 12<br/>主键 201-300]
    C -->|next| D[叶子节点 13<br/>主键 301-400]
    D -->|next| E[叶子节点 14<br/>主键 401-500]
    E -->|prev| D
    D -->|prev| C
    C -->|prev| B
    B -->|prev| A

    subgraph "范围扫描利用双向链表"
        F[SELECT * FROM users<br/>WHERE id BETWEEN 150 AND 350]
        F --> G[定位到 150 所在的叶子节点 11]
        G --> H[通过 next 指针<br/>从节点 11 → 12 → 13]
        H --> I[在节点 13 的 350 处停止]
    end
```

## Page Split

当插入导致页面满时，InnoDB 触发 Page Split，将页面分裂为两个，并把分隔键提升到父节点。

### Page Split 流程

```mermaid
sequenceDiagram
    participant Insert as 插入操作
    participant FullPage as 满页 P
    participant NewPage as 新页 Q
    participant Parent as 父节点

    Insert->>FullPage: 插入行，发现页面已满
    FullPage->>FullPage: 确定分裂点<br/>（通常为 50% 位置）
    Insert->>NewPage: 分配新页面 Q
    Insert->>FullPage: 移动一半行到 Q
    Insert->>FullPage: 更新 Q 的 prev 指针指向 P
    Insert->>FullPage: 更新 P 的 next 指针指向 Q
    Insert->>FullPage: 更新 Q 的 next 指针指向 P 的原 next
    
    Note over Insert,Parent: 将分隔键插入父节点
    Insert->>Parent: 将分隔键（Q 的最小键）插入父节点
    Parent->>Parent: 父节点检查是否满
    Parent-->>Insert: 父节点空间足够，插入完成
```

### Page Split 前后状态

```mermaid
graph TD
    subgraph "分裂前（Page 已满）"
        Before[页面 P<br/>主键 1-200<br/>已满，无法插入]
    end

    subgraph "分裂后（两个页面）"
        AfterP[页面 P<br/>主键 1-100<br/>50% 利用率]
        AfterQ[页面 Q<br/>主键 101-200<br/>50% 利用率]
    end

    Before -->|插入操作触发分裂| AfterP
    Before -->|插入操作触发分裂| AfterQ

    AfterP -->|next| AfterQ
    AfterQ -->|prev| AfterP

    subgraph "父节点变化"
        ParentBefore[父节点: ... 键值 200 ...]
        ParentAfter[父节点: ... 键值 100, 键值 200 ...]
    end

    ParentBefore -->|插入分隔键 100| ParentAfter
```

## Page Merge

当页面的利用率低于 50%（通常由大量删除操作引起）时，InnoDB 会尝试与相邻页面合并。

### Page Merge 流程

```mermaid
flowchart TD
    A[删除操作<br/>导致页面利用率低] --> B[检查是否触发合并条件]
    B --> C{页面利用率 < MERGE_THRESHOLD?}
    C -->|是| D[尝试与左兄弟合并]
    C -->|否| E[不合并]
    
    D --> F{左兄弟空间足够<br/>容纳当前页?}
    F -->|是| G[合并到左兄弟]
    F -->|否| H[尝试与右兄弟合并]
    
    H --> I{右兄弟空间足够<br/>容纳当前页?}
    I -->|是| J[合并到右兄弟]
    I -->|否| E
    
    G --> K[更新链表指针]
    J --> K
    K --> L[释放当前页<br/>标记为空闲]
    L --> M[删减父节点中的分隔键]
```

**MERGE_THRESHOLD**：MySQL 8.0 引入，通过 `INDEX_MERGE_THRESHOLD` 参数控制（默认 50%）。

## 索引组织表 vs Heap 表

InnoDB 是**索引组织表（Index Organized Table, IOT）**，数据按主键顺序存储在聚簇索引中。这与 PostgreSQL 的 Heap 表有本质区别。

```mermaid
graph TD
    subgraph "InnoDB 索引组织表"
        IOT[表 = 聚簇索引]
        IOT_A[聚簇索引<br/>叶子节点存储完整行]
        IOT_B[二级索引<br/>叶子节点存储主键]
        IOT_A --> IOT_B
        IOT_B --> IOT_A[回表查找]
    end

    subgraph "PostgreSQL Heap 表"
        Heap[表 = Heap 文件]
        Heap_A[Heap 表<br/>行数据无序存储]
        Heap_B[索引<br/>叶子节点存储 TID]
        Heap_C[索引<br/>叶子节点存储 TID]
        Heap_A --> Heap_B
        Heap_A --> Heap_C
        Heap_B --> Heap_A[TID 直接定位行]
        Heap_C --> Heap_A[TID 直接定位行]
    end
```

### 对比表

| 维度 | InnoDB 索引组织表 | PostgreSQL Heap 表 |
|------|------------------|-------------------|
| 数据存储 | 聚簇索引叶子节点 | Heap 文件（独立堆） |
| 主键索引 | 聚簇索引（存完整行） | B+Tree（存 TID） |
| 二级索引 | 存主键值，需回表 | 存 TID，直接定位 |
| 行顺序 | 按主键排序 | 无序（插入顺序或空位） |
| 主键变更 | 代价极高（重写整行） | 代价低（更新索引项） |
| 空间利用率 | 高（无独立 Heap） | 中（有 Heap 元组头） |
| 范围扫描 | 主键范围扫描极快 | 需要索引扫描 + 回表 |
| 插入性能 | 主键顺序插入快，随机插入慢 | 插入位置无关 |

### 索引组织表的优势

```sql
-- 1. 主键范围扫描快
SELECT * FROM users WHERE id BETWEEN 100 AND 200;
-- 定位到 100 的叶子节点，通过 next 指针遍历到 200
-- 无需回表，所有数据就在叶子节点上

-- 2. 无额外 Heap 存储
-- 表数据就是聚簇索引，不需要独立的文件存储行数据

-- 3. 聚簇索引本身就是主键索引
-- 主键查询一步到位，不需要索引 + 回表两阶段
```

### 索引组织表的劣势

```sql
-- 1. 二级索引占用空间大（包含主键值）
-- 主键越大，所有二级索引越大
-- 建议使用自增整数主键，避免 UUID 或长字符串

-- 2. 随机主键插入导致 Page Split
-- UUID 主键随机插入，频繁 Page Split
INSERT INTO users (id, name) VALUES (UUID(), 'Alice');  -- 性能差

-- 3. 主键修改代价高
UPDATE users SET id = 200 WHERE id = 100;
-- 需要删除旧行 + 插入新行 + 更新所有二级索引
```

## 主键选择建议

```mermaid
flowchart TD
    A[主键选择] --> B{有自然主键?}
    B -->|是| C{自然主键是<br/>自增整数?}
    B -->|否| D[使用自增整数<br/>AUTO_INCREMENT]
    
    C -->|是| E[可以使用<br/>如 id INT AUTO_INCREMENT]
    C -->|否| F[建议使用自增代理主键<br/>自增 INT/BIGINT]
    
    D --> G[优点: 插入顺序递增<br/>无 Page Split 问题]
    D --> H[优点: 二级索引空间小<br/>4 字节 vs 16 字节 UUID]
    
    F --> I[UUID 作为主键的缺点]
    I --> J[Page Split 频繁<br/>插入性能下降]
    I --> K[索引碎片化<br/>空间利用率低]
    I --> L[二级索引空间大<br/>16 字节主键]
    
    E --> M[避免: 长字符串主键]
    E --> N[避免: 频繁更新的列]
```

## 要点总结

- InnoDB 使用 **B+Tree**（不是 B-Tree），叶子节点形成双向链表，支持高效范围扫描
- **聚簇索引**：主键就是聚簇索引，叶子节点存储完整行数据，数据即索引
- **二级索引**：叶子节点存储索引列 + 主键值，查询需要回表（通过主键查找聚簇索引）
- **覆盖索引**：索引包含所有查询列，不需要回表，Extra 显示 `Using index`
- **Page Split**：插入导致页面满时分裂为两个页面，可能影响插入性能
- **Page Merge**：删除导致页面利用率低于 50% 时尝试合并
- **索引组织表 vs Heap 表**：InnoDB 存储行数据在聚簇索引中，PG 存储在独立的 Heap 中
- 主键建议使用 **自增整数**，避免 UUID 或长字符串

## 思考题

1. 为什么 InnoDB 选择 B+Tree 而不是 B-Tree？B+Tree 的叶子节点双向链表对范围扫描有什么意义？
2. 二级索引存储主键值（而不是行指针）的设计决策带来了什么利弊？与 PG 的 TID 方案相比哪个更好？
3. 如果一张表的主键是 UUID，插入性能会严重下降。除了使用自增整数外，还有哪些优化方案？
4. InnoDB 的 Page Split 通常是 50% 分裂，但 MySQL 8.0 引入了批量分裂优化。解释批量分裂的原理和优势。