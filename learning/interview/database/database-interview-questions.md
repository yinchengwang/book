# 数据库/SQL 面试题（含参考答案）

> 本文档覆盖关系型数据库从基础原理到分布式实践的完整知识体系。
> 共 **7 个维度、35 道题**，按难度分为 L1（基础）→ L4（进阶）。
> 配套基础问答见 [questions.md](questions.md)，SQL 语法练习见 [SQL.md](SQL.md)。

---

## 维度一：事务与并发控制（L1-L3）

### Q1：事务的 ACID 分别指什么？MySQL InnoDB 是如何实现它们的？

**难度**：L1

**参考答案**：

| 特性 | 含义 | InnoDB 实现方式 |
|---|---|---|
| **A**tomicity（原子性） | 事务要么全做，要么全不做 | **Undo Log**：事务回滚时，根据 Undo Log 逆向执行操作 |
| **C**onsistency（一致性） | 事务前后数据完整性约束一致 | 由应用层 + 数据库约束共同保证（原子性、隔离性、持久性是手段，一致性是目的） |
| **I**solation（隔离性） | 并发事务互不干扰 | **MVCC** + **锁机制**（见 Q2/Q3） |
| **D**urability（持久性） | 提交后数据不会丢失 | **Redo Log**（WAL 机制）：事务提交时先写 Redo Log，再写数据页。崩溃恢复时重放 Redo Log |

**关键理解**：Redo Log 保证持久性（写入即落盘），Undo Log 保证原子性（回滚时恢复旧版本），两者共同支撑崩溃恢复。

---

### Q2：四种事务隔离级别分别解决了什么问题？MySQL 默认是什么？

**难度**：L1

**参考答案**：

| 隔离级别 | 脏读 | 不可重复读 | 幻读 | 实现方式 |
|---|---|---|---|---|
| READ UNCOMMITTED | ❌ 可能 | ❌ 可能 | ❌ 可能 | 直接读最新版本 |
| READ COMMITTED | ✅ 避免 | ❌ 可能 | ❌ 可能 | 每次读时重新生成 ReadView |
| REPEATABLE READ | ✅ 避免 | ✅ 避免 | ⚠️ 部分避免 | 事务开始时生成 ReadView，整个事务复用 |
| SERIALIZABLE | ✅ 避免 | ✅ 避免 | ✅ 避免 | 所有读加共享锁，写加排他锁 |

**MySQL InnoDB 默认隔离级别**：**REPEATABLE READ**

**特别注意**：MySQL 的 RR 级别通过 **Next-Key Lock** 解决了幻读问题（标准 SQL 规范中 RR 不解决幻读，但 InnoDB 做到了）。所以 MySQL 的 RR ≈ 标准 SQL 的可序列化。

> **脏读**：读到另一个事务未提交的数据
> **不可重复读**：同一个事务内两次读取同一行，结果不一致（行变化）
> **幻读**：同一个事务内两次范围查询，结果集行数不一致（新增/删除行）

---

### Q3：MVCC 的原理是什么？ReadView 怎么判断事务可见性？

**难度**：L2

**参考答案**：

**核心数据结构**：每行记录有两个隐藏列：
- `DB_TRX_ID`：最近修改该行的事务 ID
- `DB_ROLL_PTR`：指向 Undo Log 的指针，用于找到旧版本

**ReadView 的组成**：
- `m_low_limit_id`：当前活跃事务中最大事务 ID + 1
- `m_up_limit_id`：当前活跃事务中最小事务 ID
- `m_ids`：当前所有活跃事务 ID 列表
- `m_creator_trx_id`：创建该 ReadView 的事务 ID

**可见性判断算法**（伪代码）：

```
function is_visible(trx_id, readview):
    if trx_id == readview.m_creator_trx_id:
        return TRUE          // 自己改的，可见
    if trx_id < readview.m_up_limit_id:
        return TRUE          // 在活跃列表前已提交，可见
    if trx_id >= readview.m_low_limit_id:
        return FALSE         // 在 ReadView 创建后才启动，不可见
    if trx_id in readview.m_ids:
        return FALSE         // 在活跃列表中，未提交，不可见
    return TRUE              // 已提交，可见
```

**RC vs RR 的关键区别**：
- **RC**：每条语句执行前重新生成 ReadView
- **RR**：只在事务第一条语句执行时生成 ReadView，后续复用

所以同样的 SQL，RC 可能看到其他事务新提交的数据（不可重复读），RR 看不到（可重复读）。

---

### Q4：MySQL 有哪些行锁？Next-Key Lock 解决什么问题？

**难度**：L2

**参考答案**：

**行锁类型**：

| 锁类型 | 含义 | SQL 示例 |
|---|---|---|
| Record Lock | 单行记录锁 | `SELECT ... WHERE id=1 FOR UPDATE` |
| Gap Lock | 间隙锁（锁定两条记录之间的间隙） | `SELECT ... WHERE id BETWEEN 1 AND 10 FOR UPDATE` |
| Next-Key Lock | Record Lock + Gap Lock 的组合 | InnoDB RR 级别下默认使用 |

**Next-Key Lock 解决幻读**：

当执行 `SELECT ... WHERE id > 5 FOR UPDATE` 时，InnoDB 不光锁住已有的行（id=6,7,8...），还会锁住这些行之间的**间隙**，阻止其他事务在间隙中插入新行。

```
实际数据：id: [1, 3, 5, 8, 10]
Next-Key Lock 范围：(-∞, 1], (1, 3], (3, 5], (5, 8], (8, 10], (10, +∞)
                           ↑——这里插不进去
```

**优化**：唯一索引上的等值查询会退化为 Record Lock（因为确定唯一行，不需要锁间隙）。

> **坑点**：无索引字段做条件查询时，InnoDB 会锁住所有行（相当于表锁）！因为无法通过索引找到精确的行和间隙。

---

### Q5：死锁是怎么产生的？怎么排查和避免？

**难度**：L2

**参考答案**：

**经典死锁场景**：

```
事务 A: UPDATE account SET balance=100 WHERE id=1;
事务 B: UPDATE account SET balance=200 WHERE id=2;
事务 A: UPDATE account SET balance=50 WHERE id=2;  -- 等待 B 释放 id=2 的锁
事务 B: UPDATE account SET balance=300 WHERE id=1;  -- 等待 A 释放 id=1 的锁
                                                      -- 死锁！
```

**排查方式**：

```sql
-- 查看最近一次死锁信息
SHOW ENGINE INNODB STATUS\G

-- 查看当前事务列表
SELECT * FROM INFORMATION_SCHEMA.INNODB_TRX;

-- 查看锁等待
SELECT * FROM INFORMATION_SCHEMA.INNODB_LOCK_WAITS;
```

**避免策略**：
1. **固定访问顺序**：所有事务按相同顺序访问表/行（如按 id 升序）
2. **缩小锁范围**：走索引 → Record Lock 而非 Gap Lock
3. **缩短事务时间**：不要在事务中做外部 API 调用、用户输入等待
4. **降低隔离级别**：RC 级别没有 Gap Lock，死锁概率大幅降低
5. **设置超时**：`innodb_lock_wait_timeout`（默认 50s）

**InnoDB 死锁处理**：InnoDB 会自动检测死锁，回滚事务量较小的那个事务（牺牲小事务）。

---

## 维度二：索引原理与优化（L1-L3）

### Q6：B+树索引的结构？为什么 MySQL 用 B+树而不是 B 树、AVL 树、哈希表？

**难度**：L1

**参考答案**：

| 数据结构 | 为什么不行/合适 |
|---|---|
| **哈希表** | O(1) 等值查询，但**不支持范围查询**（>、<、BETWEEN）和排序。Hash 碰撞时性能退化 |
| **AVL/红黑树** | 树高 log₂N，1000 万条数据树高约 24。而 B+树扇出大（每个节点约 1200 个 key），树高仅 3-4 层。另外 AVL 每次插入可能多次旋转 |
| **B 树** | 内部节点也存数据，导致节点扇出小，树更高。且范围查询需要中序遍历，无法像 B+树那样链表扫描 |
| **B+树** ✅ | 非叶子节点只存 key 不存数据，扇出大 → 树矮（3-4 层）。叶子节点形成有序链表，**范围查询和排序极快** |

**B+树关键特性（InnoDB）**：
- 根节点常驻内存，实际读取只需要 2-3 次磁盘 IO
- 叶子节点是双向链表，支持正向/反向范围扫描
- 每个节点对应一个 **Page（16KB）**，I/O 友好

---

### Q7：聚簇索引、二级索引、覆盖索引的区别？什么是回表？

**难度**：L1

**参考答案**：

**聚簇索引（Clustered Index）**：
- InnoDB 表中必有且只有一个：主键即聚簇索引
- 叶子节点存储**整行数据**
- 如果表没有主键，InnoDB 选第一个 UNIQUE NOT NULL 列；如果也没有，隐式创建 `ROW_ID`

**二级索引（Secondary Index）**：
- 叶子节点存储**主键值**（不是行指针）
- 通过二级索引查询时：先找到主键 → 再通过聚簇索引定位到完整行

**回表（Book Lookup）**：
```
SELECT * FROM user WHERE name = '张三';  -- name 有索引
    1. 在 name 索引树找到主键 id = 10
    2. 再到聚簇索引树找到 id=10 的完整行  ← 这个过程叫回表
```

**覆盖索引（Covering Index）**：
```
CREATE INDEX idx_name_age ON user(name, age);  -- 联合索引

SELECT name, age FROM user WHERE name = '张三';  
-- 注意：只查 name 和 age，这两个字段在 idx_name_age 中都有了
-- 直接返回，不需要回表 → Extra 列显示 "Using index"
```

**性能差异**：
- 回表一次：1 次二级索引 IO + 1 次聚簇索引 IO = 2 次 IO
- 回表 N 行：如果二级索引找到 N 条记录，最多 N 次回表

---

### Q8：联合索引的最左前缀原则是什么？什么情况下索引会失效？

**难度**：L1

**参考答案**：

**最左前缀原则**：联合索引 `(a, b, c)` 能匹配的场景：

| WHERE 条件 | 能否用到索引 | 说明 |
|---|---|---|
| `WHERE a=1` | ✅ 全用到 | 匹配第一列 |
| `WHERE a=1 AND b=2` | ✅ 全用到 | 匹配前两列 |
| `WHERE a=1 AND b=2 AND c=3` | ✅ 全用到 | 全匹配 |
| `WHERE b=2` | ❌ | 跳过了最左列 a |
| `WHERE a=1 AND c=3` | ⚠️ 部分用到 | 只用到了 a 列（c 不能走索引） |
| `WHERE a=1 AND b>2 AND c=3` | ⚠️ 部分用到 | 只用到了 a 和 b（b 上做了范围查询后，c 失效） |

**索引失效的常见场景**：

```
① 隐式类型转换
   SELECT * FROM user WHERE phone = 13800000000;  
   -- phone 是 VARCHAR，传入数字，MySQL 会把 phone 转换成数字 → 索引失效

② 对索引列做函数操作
   SELECT * FROM user WHERE YEAR(create_time) = 2023;  
   -- 应该写成：create_time BETWEEN '2023-01-01' AND '2023-12-31'

③ 使用 != 或 NOT IN（大概率失效，取决于数据分布）
   SELECT * FROM user WHERE status != 1;  -- MySQL 优化器判断扫描成本更低

④ LIKE '%xxx' 左模糊
   SELECT * FROM user WHERE name LIKE '%张三';  -- 失效
   SELECT * FROM user WHERE name LIKE '张三%';  -- 有效（范围查询）

⑤ OR 条件中部分字段无索引
   SELECT * FROM user WHERE name = '张三' OR phone = '138xxx';  
   -- phone 无索引 → 全表扫描
```

---

### Q9：什么是索引下推（Index Condition Pushdown, ICP）？有什么好处？

**难度**：L2

**参考答案**：

**没有 ICP 时**：
```
INDEX idx_name_age(name, age)
SELECT * FROM user WHERE name LIKE '张%' AND age = 25;

1. 在二级索引中找到所有 name LIKE '张%' 的记录（可能 1000 条）
2. 对这 1000 条逐一回表，找到完整行
3. 再过滤 age = 25（回表后才过滤）
```

**有 ICP 时**：
```
1. 在二级索引中同时检查 name LIKE '张%' AND age = 25
   → 只有 age=25 的记录才回表（可能只剩 10 条）
2. 只回表 10 次
```

**性能提升**：**ICP 减少了回表次数**。联合索引但最右列无法参与搜索时（如范围查询、LIKE 前缀匹配），ICP 让"条件过滤"下推到存储引擎层，在索引层面完成，不需要回表。

**查看方式**：EXPLAIN 的 Extra 列显示 `"Using index condition"`。

**MySQL 版本**：5.6+ 引入，默认开启。可通过 `SET optimizer_switch = 'index_condition_pushdown=off'` 关闭。

---

### Q10：怎么看 MySQL 的执行计划？重点关注哪些字段？

**难度**：L2

**参考答案**：

```sql
EXPLAIN SELECT u.name, o.amount FROM user u 
  JOIN order o ON u.id = o.user_id 
  WHERE u.age > 18 ORDER BY o.create_time DESC LIMIT 10;
```

**关键字段**：

| 字段 | 重点关注 |
|---|---|
| **type** | ALL（全表扫描 ❌）→ index → range → ref → eq_ref → const（最快 ✅） |
| **key** | 实际使用的索引（如果为 NULL，说明没用到索引） |
| **rows** | MySQL 估算需要扫描的行数（越小越好，偏差太大说明统计信息过期） |
| **Extra** | `Using filesort`（需要额外排序 ❌）、`Using temporary`（使用了临时表 ❌）、`Using index`（覆盖索引 ✅）、`Using index condition`（索引下推 ✅） |
| **key_len** | 索引使用的字节数（联合索引中可以判断实际使用了哪几列） |

**常见优化方向**：
- `type=ALL` → 必须加索引
- `Extra=Using filesort` → ORDER BY 字段需要索引
- `Extra=Using temporary` → GROUP BY 或 DISTINCT 需要优化
- `rows` 偏差大 → `ANALYZE TABLE` 更新统计信息

---

### Q11：MySQL 的 JOIN 是怎么实现的？NLJ、BNL、Hash Join？

**难度**：L2

**参考答案**：

**MySQL 有三种 JOIN 实现方式**：

```
① Nested Loop Join（NLJ）—— 小表驱动大表，逐行匹配
    for each row in t1:           -- 驱动表
        for each row in t2:       -- 被驱动表（有索引时走索引查找）
            if match: output

   时间复杂度：O(R 行数 × S 行数)，被驱动表有索引时 O(R × logS)
   适用：被驱动表有索引，数据量不大

② Block Nested Loop（BNL）—— 批量拉取，减少 IO
   把 t1 一次读一批（join_buffer_size 控制），批量去匹配 t2
   适用：被驱动表**无索引**，但 MySQL 5.7- 的主要方案

③ Hash Join —— MySQL 8.0.18+ 引入
   把 t1 读入内存构建哈希表，t2 逐行去哈希表中探测
   适用：两表都**无对应索引**，且 t1 足够小能放入内存 join_buffer
   时间复杂度：O(R + S)，通常比 BNL 快
```

**面试追问：驱动表怎么选？**
- MySQL 一般选**小表**做驱动表（优化器估算 rows 小的那个）
- 如果用错了，可以用 `STRAIGHT_JOIN` 强制指定驱动表

---

## 维度三：高级 SQL（L2-L3）

### Q12：窗口函数有哪些？和 GROUP BY 的区别？

**难度**：L2

**参考答案**：

**核心区别**：

```
GROUP BY：对数据分组聚合，每组返回一行
窗口函数：每一行保留，同时可以访问"窗口"内的其他行数据
```

**常见窗口函数**：

| 类别 | 函数 | 用途 |
|---|---|---|
| 排名 | `ROW_NUMBER()` | 唯一排名（不重复） |
| 排名 | `RANK()` | 并列排名，跳过后续（1,1,3） |
| 排名 | `DENSE_RANK()` | 并列排名，不跳过（1,1,2） |
| 聚合 | `SUM() OVER(...)` | 累计求和（移动汇总） |
| 聚合 | `AVG() OVER(...)` | 移动平均 |
| 取值 | `LAG(col, n)` | 取前 n 行的值 |
| 取值 | `LEAD(col, n)` | 取后 n 行的值 |
| 取值 | `FIRST_VALUE(col)` | 窗口内第一行的值 |
| 取值 | `LAST_VALUE(col)` | 窗口内最后一行的值 |

**经典场景** - 分组取 Top-N：

```sql
-- 每个部门工资最高的 3 个人
SELECT * FROM (
  SELECT *, ROW_NUMBER() OVER (PARTITION BY dept_id ORDER BY salary DESC) AS rk
  FROM employee
) t WHERE rk <= 3;
```

---

### Q13：CTE 和递归 CTE 的应用场景？

**难度**：L2

**参考答案**：

**CTE（公用表表达式）**：
```sql
WITH dept_avg AS (
  SELECT dept_id, AVG(salary) AS avg_sal
  FROM employee GROUP BY dept_id
)
SELECT e.*, d.avg_sal
FROM employee e JOIN dept_avg d ON e.dept_id = d.dept_id
WHERE e.salary > d.avg_sal;
```

CTE 比子查询好的地方：**可读性更强，可被多次引用**。

**递归 CTE** - 处理树形/层级结构：

```sql
-- 查找所有子部门（典型的树形结构）
WITH RECURSIVE sub_depts AS (
  -- 锚点：顶级部门
  SELECT id, name, parent_id, 1 AS level
  FROM department WHERE id = 1

  UNION ALL

  -- 递归：找子部门
  SELECT d.id, d.name, d.parent_id, sd.level + 1
  FROM department d JOIN sub_depts sd ON d.parent_id = sd.id
)
SELECT * FROM sub_depts;
```

**适用场景**：组织架构树、BOM（物料清单）、分类目录、评论回复链。

**性能注意**：递归 CTE 默认最大递归深度 1000（可用 `cte_max_recursion_depth` 调大）。

---

### Q14：SQL 查询优化的核心思路？从执行计划反推优化？

**难度**：L2

**参考答案**：

**优化核心思路**：**减少扫描行数，减少回表次数。**

**优化步骤**：

```
① 查看执行计划（EXPLAIN）→ 找出 type=ALL 或 Extra 含 filesort/temporary 的查询
② 确认访问的行数是否合理 → 如果 rows 很大说明索引没用上
③ 思考 index 的最佳选择 → 覆盖索引 > 减少回表 > 索引过滤
④ 检查排序/分组能否走索引 → ORDER BY/GROUP BY 字段加索引
⑤ 考虑改写 SQL → 分页优化（游标分页 vs offset）、NOT IN 改 JOIN
```

**经典案例** - 深分页优化：

```sql
-- 原写法（MySQL 需要扫描 100000 行后丢弃前 99990 行）
SELECT * FROM order LIMIT 99990, 10;

-- 优化后（游标分页）
SELECT * FROM order WHERE id > 99990 LIMIT 10;

-- 或者延迟关联（先查主键再 JOIN 原表）
SELECT * FROM order o
INNER JOIN (SELECT id FROM order ORDER BY id LIMIT 99990, 10) tmp
ON o.id = tmp.id;
```

---

## 维度四：存储引擎与架构（L2-L3）

### Q15：InnoDB 的 Buffer Pool 是怎么工作的？LRU 算法的改进？

**难度**：L2

**参考答案**：

**Buffer Pool 作用**：缓存数据页，减少磁盘 IO。是 InnoDB 性能的核心。

**大小**：`innodb_buffer_pool_size`，建议设为物理内存的 60-80%。

**InnoDB 对 LRU 的改进**：传统 LRU 有个问题——**预读污染**。InnoDB 做了**冷热分区**：

```
[ 热区 (5/8) | 冷区 (3/8) ]
    ↑               ↑
  频繁访问         新读入页面

① 新读入的页面放在冷区头部
② 如果页面在冷区被访问 2 次以上，才移至热区
③ 热区页面长时间未被访问，降级到冷区

防止全表扫描/预读把热数据刷出
```

**相关参数**：
- `innodb_old_blocks_time`：页面在冷区的"保护时间"（默认 1000ms），期间访问不计入提升
- `innodb_old_blocks_pct`：冷区比例（默认 37%，即 3/8）

---

### Q16：Redo Log、Undo Log、Binlog 的区别和协作关系？

**难度**：L2

**参考答案**：

| | Redo Log | Undo Log | Binlog |
|---|---|---|---|
| **层级** | InnoDB 存储引擎层 | InnoDB 存储引擎层 | MySQL Server 层 |
| **作用** | **持久性**——崩溃恢复重放 | **原子性**——MVCC 可见性 + 回滚 | **复制+恢复**——主从同步、PITR |
| **内容** | 物理日志（页/页偏移/修改内容） | 逻辑日志（记录修改前的旧版本） | SQL 语句（STATEMENT）或行变更（ROW） |
| **写入时机** | 事务提交时写入（WAL） | 修改回滚段，随事务推进 | 事务提交时写入（所有引擎共享） |
| **清理** | 循环写（checkpoint 后覆盖） | 没有事务引用旧版本时可以清理 | 保留期后可手动清理 |

**两阶段提交（保证 Redo Log 和 Binlog 一致）**：

```
事务提交流程：
                     Redo Log (prepare)
                      /         \
                 ╱                   ╲
            写入成功                 写入失败 → 回滚
                │
            Binlog 写入
                │
                ↓
            Redo Log (commit)
                
崩溃恢复场景：
1. Redo Log prepare + Binlog 完整 → 提交事务（两者一致）
2. Redo Log prepare + Binlog 不完整 → 回滚事务（避免数据重复）
```

---

### Q17：Change Buffer 解决了什么问题？什么场景下无效？

**难度**：L2

**参考答案**：

**解决的问题**：当更新**二级索引**时，如果目标数据页不在 Buffer Pool 中，不需要立即从磁盘读取该页，而是将变更缓存在 Change Buffer 中。等下次需要读该页时，再 merge 进去。

**收益**：将随机读延迟合并，减少磁盘 IO。对**写密集 + 二级索引多**的场景效果显著。

**无效场景**：
1. **唯一索引**：需要检查唯一性，必须读页判断，Change Buffer 不生效。这也是**普通索引和唯一索引在写性能上的关键差异**
2. **索引列上没有 UPDATE/DELETE/INSERT**：Change Buffer 只针对二级索引的变更操作
3. **页已经在 Buffer Pool 中**：直接更新页即可，不需要 Change Buffer

**相关参数**：
- `innodb_change_buffer_max_size`：Change Buffer 占 Buffer Pool 的百分比（默认 25%）

---

### Q18：Online DDL 是怎么做到的？不同数据库的差异？

**难度**：L3

**参考答案**：

**MySQL Online DDL（inplace vs copy）**：

```sql
-- MySQL 8.0 支持的 Instant DDL
ALTER TABLE t ADD COLUMN c INT NOT NULL DEFAULT 0, ALGORITHM=INSTANT;
-- INSTANT：仅修改元数据，秒级完成（仅支持部分操作）

-- 传统的 inplace DDL（如加二级索引）
ALTER TABLE t ADD INDEX idx_name(name), ALGORITHM=INPLACE;
-- 需要重建表，但不阻塞 DML（支持并发读写）
```

**各种操作的支持情况**：

| 操作 | ALGORITHM | 是否阻塞写 |
|---|---|---|
| 加二级索引 | INPLACE | 不阻塞（DML 可并发） |
| 加普通列（默认值） | INSTANT（8.0+） | 不阻塞 |
| 加 AUTO_INCREMENT | INSTANT | 不阻塞 |
| 修改列类型 | COPY | 阻塞（需要重建所有行） |
| 删除列 | INSTANT（8.0+ 部分支持） | 不阻塞 |
| 修改列名 | INSTANT | 不阻塞 |
| 重命名索引 | INSTANT | 不阻塞 |

**PostgreSQL vs MySQL**：
- **PG 的 DDL 靠事务性 DDL + 锁队列**：`ALTER TABLE ADD COLUMN` 只改元数据，快速完成不阻塞
- **PG 9.6+ 引入 `CREATE INDEX CONCURRENTLY`**：不阻塞写但需要两倍写放大（建两个索引，最终 drop 旧的）

---

## 维度五：高可用与扩展（L3-L4）

### Q19：MySQL 主从复制的原理？半同步复制解决了什么问题？

**难度**：L2

**参考答案**：

**复制流程**：

```
          Master                           Slave
   SQL 执行 → Binlog 写入       IO 线程 → Relay Log → SQL 线程重放
              │                     ↑
              │                     │
              └── 网络传输 ──────────┘
```

三种复制模式：

| 模式 | 原理 | 强项 | 弱项 |
|---|---|---|---|
| **异步复制** | Master 不等待 Slave 确认 | 性能最好 | 主宕机丢数据 |
| **半同步复制**（rpl_semi_sync） | Master 等待至少 1 个 Slave 确认收到 Binlog 后才返回客户端 OK | 性能损耗小，数据不丢 | Slave 挂了 Master 降级为异步 |
| **全同步复制** | Master 等待所有 Slave 确认 | 数据完全一致 | 性能极差 |

**半同步复制的关键点**：只是保证 Binlog **已传输到 Slave**，不保证 SQL 线程已经重放。Slave 宕机前可能有 Relay Log 未重放，重启后会自动恢复。

---

### Q20：分库分表的策略？分片键如何选择？跨分片查询怎么处理？

**难度**：L3

**参考答案**：

**分片策略**：

| 策略 | 方式 | 优点 | 缺点 |
|---|---|---|---|
| **Range 分片** | user_id 1-1000→库1，1001-2000→库2 | 扩展简单，范围查询友好 | 热点集中（新数据集中写最新库） |
| **Hash 分片** | user_id % 16 → 库 user_id/16 % 16 → 表 | 数据均匀分布 | 扩缩容需重新哈希，跨分片查询难 |
| **一致性哈希** | 虚拟节点 + 哈希环 | 扩缩容影响小 | 实现复杂 |

**分片键选择原则**：
1. **尽量覆盖 90%+ 的查询条件**——比如按 user_id 分片，所有业务查询都带 user_id
2. **均匀分布**——避免热点（如按地区分片，北京上海数据量远超其他地区）
3. **不可变或极少变化**——分片键变了意味着数据迁移

**跨分片查询**：

```
① 全局聚合：所有分片返回结果，应用层合并排序（天然适合 order by+limit）
② 广播查询：不带分片键时，请求广播到所有分片
③ 全局表方案：字典/配置表在每个分片都完全复制
④ ES 辅助：跨分片搜索走 ES，返回主键 ID 再去分片拉取详情
```

**业界主流方案**：

| 中间件 | 特点 |
|---|---|
| **ShardingSphere** | 应用层 JDBC 代理，支持分片 + 读写分离 + 分布式事务 |
| **MyCAT** | 数据库代理层，透明分片 |
| **Vitess** | YouTube 开源，Kubernetes 原生，支持大规模分片 |
| **TiDB** | 自动分片（NewSQL），对应用完全透明 |

---

### Q21：CAP 理论在数据库中的体现？传统数据库和分布式数据库各自的选择？

**难度**：L2

**参考答案**：

**CAP 三角形**：C（强一致性）A（可用性）P（分区容忍性）三者最多同时满足两个。

```
                    C（强一致性）
                   /\
                  /  \
                 /    \
                /      \
               /________\
              A          P
           （可用性）  （分区容忍性）
```

**数据库的 CAP 选择**：

| 数据库类型 | 选择 | 代表产品 | 说明 |
|---|---|---|---|
| 传统单机 | CA | MySQL、PG（单机） | 网络不分区，保证 C 和 A |
| 分布式数据库（CP）| C + P | TiDB、CockroachDB | 优先一致性，分区后牺牲部分可用性 |
| 分布式缓存（AP）| A + P | Redis Cluster、DynamoDB | 优先可用性，最终一致性 |
| NoSQL | AP | Cassandra、CouchDB | AP 优先 |

**不是非黑即白**：大部分分布式数据库实际提供**可调一致性**（Tunable Consistency）：
- 强一致性（Linearizability）→ 性能差
- 因果一致性 → 折中
- 最终一致性 → 性能好

例如：TiDB 默认强一致（基于 Raft + Percolator），但你可以在读取时设置 `tidb_replica_read=leader_or_follower` 牺牲一致性换取更低延迟。

---

## 维度六：分布式数据库（L3-L4）

### Q22：Raft 协议的核心流程？Leader 挂了怎么办？

**难度**：L3

**参考答案**：

**Raft 的三个角色**：

| 角色 | 行为 |
|---|---|
| **Leader** | 处理所有客户端请求，负责日志复制 |
| **Follower** | 被动接收 Leader 的日志 |
| **Candidate** | 选举过程中的过渡角色 |

**日志复制流程**：

```
客户端 → Leader 接收请求
  → Leader 写入本机日志（未提交）
  → Leader 向所有 Follower 发送 AppendEntries RPC
  → 大多数 Follower 写入成功后，Leader 提交日志
  → Leader 回复客户端 OK
  → Leader 通知 Follower 该日志已提交
```

**Leader 选举**：

```
① Follower 在选举超时时间（150-300ms 随机）内未收到 Leader 心跳
② Follower 转换为 Candidate，自增 term，给自己投票
③ 向其他节点发送 RequestVote RPC
④ 获得大多数投票 → 成为新 Leader
⑤ 发送心跳通知其他节点
```

**关键设计**：
- **随机超时**：避免多个 Candidate 同时选举导致 split vote
- **term 的概念**：每个任期一个 Leader，term 单调递增，过期的 RPC 被拒绝
- **日志匹配（Log Matching）**：保证所有节点的日志一致

---

### Q23：分布式事务的实现方式？2PC、3PC、TCC、Saga 各自优劣？

**难度**：L3

**参考答案**：

**2PC（两阶段提交）**：

```
阶段一（准备）：协调者问所有参与者"能提交吗？"→ 参与者写 Undo/Redo Log，回答 Yes/No
阶段二（提交）：如果全 Yes → 协调者通知提交；有 No → 通知回滚
```

| 问题 | 说明 |
|---|---|
| 协调者单点 | 协调者宕机，所有参与者阻塞等待 |
| 同步阻塞 | 阶段二完成前，参与者持有锁 |
| 脑裂 | 协调者发了提交但部分参与者没收到 → 数据不一致 |

**3PC **—— 比 2PC 多了超时机制，减少了阻塞时间，但不能完全解决脑裂。

**TCC（Try-Confirm/Cancel）**：

| 阶段 | 业务含义 |
|---|---|
| Try | 预留资源（如冻结库存、锁定账户余额） |
| Confirm | 确认执行（真正扣减） |
| Cancel | 回滚（释放预留资源） |

**优点**：无锁、无阻塞、性能好，适合高并发
**缺点**：业务侵入性强，每个操作都需要实现 Try/Confirm/Cancel 三个接口

**Saga（长事务补偿）**：

```
把一个大事务拆成多个子事务，每个子事务有对应的补偿操作

T1(扣库存) → T2(创建订单) → T3(扣款)
   ↓补偿         ↓补偿          ↓补偿
 C1(加库存) ← C2(取消订单) ← C3(退款)
```

**方式**：
- **编排式（Orchestration）**：一个协调者串行编排子任务
- ** choreography（ choreography ）**：每个服务完成后发布事件，触发下一个服务

**选型建议**：

| 方案 | 一致性 | 性能 | 业务侵入 | 适用场景 |
|---|---|---|---|---|
| 2PC/3PC | 强一致 ❗️ | 差 | 低 | 短事务、小数据量 |
| TCC | 最终一致 | 好 | 高 | 高并发、库存类 |
| Saga | 最终一致 | 好 | 中 | 长事务、跨服务（如订单流程） |
| 本地消息表 | 最终一致 | 好 | 低 | 非实时、可延迟（如通知、日志） |

---

### Q24：TiDB 的架构？它怎么兼容 MySQL 并实现水平扩展？

**难度**：L4

**参考答案**：

**TiDB 三大核心组件**：

```
                    TiDB Server（SQL 层）
                   /     |       \
                  /      |        \
            Placement Driver (PD)    TiKV（存储层 — 基于 Raft）
                                        |
                                    TiFlash（列存引擎 — 分析查询）
```

**各组件的职责**：

| 组件 | 类比 MySQL | 职责 |
|---|---|---|
| **TiDB** | Server 层 | SQL 解析、优化、执行。无状态，可水平扩展 |
| **PD** | - | 集群管理、元数据、时间戳分配、Region 调度 |
| **TiKV** | InnoDB | 行式存储，数据按 Range 切分为 Region（默认 96MB），每个 Region 三副本 Raft |
| **TiFlash** | - | 列式存储，通过 Raft Learner 实时同步 TiKV 数据 |

**水平扩展原理**：

```
Region 分裂 → 当一个 Region 超过 96MB，自动分裂为两个
Region 调度 → PD 将 Region 均匀调度到各 TiKV 节点
    
                [Table]
     Region 1(1-100)   Region 2(101-200)   Region 3(201-300)
     ┌───┐ ┌───┐      ┌───┐ ┌───┐        ┌───┐ ┌───┐
     │N1 │ │N2 │      │N1 │ │N3 │        │N2 │ │N3 │
     └───┘ └───┘      └───┘ └───┘        └───┘ └───┘
     N1: 2 Region               N2: 2 Region
     N3: 1 Region → PD 调度到 N3 更多 Region
```

**MySQL 兼容性**：
- 支持 MySQL 协议，**应用层不需要改代码**（换 JDBC 连接地址即可）
- 但不支持 MySQL 的存储过程、触发器、Event
- 事务隔离级别默认 **REPEATABLE READ**（基于 Percolator 模型实现）

---

## 维度七：工程实践（L1-L4）

### Q25：线上突然出现大量慢查询，排查步骤是什么？

**难度**：L2

**参考答案**：

```
① 快速止血
   SHOW FULL PROCESSLIST;  → 查看当前运行的所有 SQL
   KILL <thread_id>;        → 杀掉消耗最大的查询

② 定位问题 SQL（三种方式）
   - 慢查询日志：SET GLOBAL slow_query_log = ON;
   - performance_schema：SELECT * FROM events_statements_summary_by_digest;
   - sys 库：SELECT * FROM sys.statement_analysis;

③ 分析执行计划
   EXPLAIN <慢 SQL> → 关注 type、rows、Extra

④ 常见原因排查
   检查项                    怎么办
   ─────────────────────────────────────────
   索引缺失/失效              → 加/重建索引
   数据量激增（统计信息过时）    → ANALYZE TABLE
   锁等待/元数据锁阻塞         → 看 INNODB_TRX/LOCK_WAITS
   参数变更                   → 检查 optimizer_switch/join_buffer_size
   网络延迟/磁盘 IO 打满       → 看 iostat、网络监控
```

---

### Q26：大表 DDL 怎么做不影响业务？

**难度**：L2

**参考答案**：

**方案对比**：

| 方案 | 优点 | 缺点 |
|---|---|---|
| **原生 INSTANT** | 秒级完成，无阻塞 | 仅支持少数操作（MySQL 8.0.12+） |
| **gh-ost**（GitHub 开源） | 对业务影响最小，可暂停可控制 | 需要 Binlog 开启 ROW 格式 |
| **pt-online-schema-change** | 成熟稳定 | 需要触发器，对主库有一定压力 |
| **低峰期直接 ALTER** | 简单 | 大表可能跑几小时阻塞写入 |

**gh-ost 工作原理**（推荐）：

```
原始表 t
  ↓ 1. 创建影子表 _t_gho（新 Schema）
  ↓ 2. 建立触发器，捕捉增量变更
  ↓ 3. 批量从 t 复制数据到 _t_gho
  ↓ 4. 完成复制后，RENAME t → _t_del, _t_gho → t
  ↓ 5. 删除旧表
```

**关键参数**：
```bash
gh-ost \
  --alter="ADD INDEX idx_name(name)" \
  --max-load=Threads_running=25 \    # 负载过高时自动暂停
  --chunk-size=1000 \                # 每次复制 1000 行
  --cut-over-lock-timeout=5          # 切换阶段锁超时
```

---

### Q27：怎么设计一个订单表？从范式化到反范式化的权衡？

**难度**：L2

**参考答案**：

**初始设计（范式化）**：

```sql
-- 订单表
CREATE TABLE `order` (
  order_id    BIGINT PRIMARY KEY,
  user_id     INT NOT NULL,
  create_time DATETIME NOT NULL,
  status      TINYINT NOT NULL,
  total_amount DECIMAL(10,2) NOT NULL
);

-- 订单明细表
CREATE TABLE order_item (
  id        BIGINT PRIMARY KEY,
  order_id  BIGINT NOT NULL,
  sku_id    INT NOT NULL,
  quantity  INT NOT NULL,
  price     DECIMAL(10,2) NOT NULL,
  FOREIGN KEY (order_id) REFERENCES `order`(order_id)
);

-- 用户表（已有）
CREATE TABLE user (
  user_id    INT PRIMARY KEY,
  username   VARCHAR(64),
  phone      VARCHAR(20)
);
```

**常见反范式化场景**：

```
需要考虑反范式化的情况            设计方案
─────────────────────────────────────────────
"订单列表需要显示用户名"     → order 表冗余 user_name（避免 JOIN）
"订单详情需要显示商品名"     → order_item 冗余 sku_name（避免 GET /sku API）
"多店铺订单"                → 分库分表键选 buyer_id 还是 seller_id？（二选一必然有跨分片查询）
"订单量极大（千万/天）"      → 按时间分表 + 冷热分离
```

**设计原则**：
1. **先按范式设计**（3NF），出性能问题再反范式
2. **反范式前先试索引优化和 SQL 改写**
3. **冗余字段的更新一致性要保证**（应用层双写或 MQ 最终一致）
4. **关键查询路径的冗余可以放心做**（读多写少的场景）

---

### Q28：数据迁移怎么做？（从一个库迁移到另一个库/从 MySQL 到 TiDB）

**难度**：L3

**参考答案**：

**方案一：停机迁移（小数据量）**

```
导出：mysqldump -h host1 db1 > data.sql
导入：mysql -h host2 db1 < data.sql

验证：逐行校验 count(*)
```

**方案二：在线迁移（大数据量，要求零停机）**：

```
阶段一：全量同步（可中断，可重试）
  └── 使用 mydumper/myloader 或 pitr 工具做全量导出导入

阶段二：增量同步（持续运行）
  └── 订阅源库 Binlog，回放到目标库
  └── 工具：DTS（云厂商）、Canal（Alibaba）、DataX、Syncer（TiDB）

阶段三：数据校验（贯穿全流程）
  └── 抽样校验：源和目标各取 1000 条记录，对比 checksum
  └── 全量校验：对大表做 count(*) + 分片 checksum

阶段四：割接
  └── 暂停源库写入 → 确认增量追平 → 切换流量 → 验证 → 开启写入
  └── 保留回滚方案：DNS 切换不改代码，5 分钟内可回滚
```

**常见坑点**：
1. **自增主键冲突**：目标库需要设更大的 auto_increment_offset
2. **字符集不一致**：源 Latin1 → 目标 UTF8，中文乱码
3. **大表 DDL**：迁移过程中源库做 DDL，Binlog 同步可能失败
4. **业务逻辑依赖**：存储过程/触发器/函数在目标库可能不支持

---

### Q29：数据库备份策略怎么设计？PITR（时间点恢复）怎么实现？

**难度**：L2

**参考答案**：

**备份类型**：

```
物理备份（XtraBackup） vs 逻辑备份（mysqldump）
  └── 物理备份更快、占用空间小、恢复更快
  └── 逻辑备份可读、可编辑、跨版本迁移方便

全量备份 vs 增量备份
  └── 全量：每周一次（周日凌晨业务低谷）
  └── 增量：每天一次
  └── Binlog：实时或准实时（秒级）
```

**推荐策略**：

```
周日 03:00  全量备份（XtraBackup）
周一～周六   增量备份或 Binlog 归档

保留周期：最近 4 周全量 + 最近 7 天 Binlog
```

**PITR（时间点恢复）步骤**：

```bash
# 1. 恢复最近的全量备份
xtrabackup --prepare --target-dir=/backup/full/
xtrabackup --copy-back --target-dir=/backup/full/

# 2. 回放 Binlog 到指定时间点
mysqlbinlog \
  --start-datetime="2026-05-14 00:00:00" \
  --stop-datetime="2026-05-14 10:30:00" \
  /var/lib/mysql/binlog.000001 \
  | mysql -u root -p
```

**RTO 和 RPO**：
- **RTO（恢复时间目标）**：从灾难发生到恢复可用所需时间 → 取决于数据量和备份方式
- **RPO（恢复点目标）**：最多丢失多少数据 → 取决于 Binlog 同步频率

---

### Q30：MySQL 的 Count(*) 为什么这么慢？不同引擎的优化思路？

**难度**：L1

**参考答案**：

**不同存储引擎的行为**：

| 引擎 | COUNT(*) 方式 | 说明 |
|---|---|---|
| **MyISAM** | 维护行数计数器（元数据）| `SELECT COUNT(*)` 直接返回，**O(1)**，但不支持 MVCC |
| **InnoDB** | 扫描索引计数 | 因为 MVCC，每个事务看到的行数可能不同，不能维护全局计数器 |

**InnoDB 的 COUNT(*) 优化**：

```
① 优先选最小的二级索引扫描（二级索引比聚簇索引小很多）
   MySQL 优化器自动选择最小的索引

② 大表优化方案：
   方案一：业务计数器表（对增删操作维护一个 count 字段）
   INSERT INTO counter_table VALUES (1, 0) ON DUPLICATE KEY UPDATE cnt = cnt + 1;

   方案二：近似估算
   SHOW TABLE STATUS LIKE 'table_name';  -- 返回 rows 近似值（有偏差）

   方案三：按天统计，缓存到 Redis

   方案四：使用 MySQL 8.0 的直方图统计
```

**注意**：`COUNT(1)` 和 `COUNT(*)` 没有性能差异，`COUNT(col)` 则排除 NULL 值。

---

### Q31：MySQL 有哪些数据类型容易踩坑？

**难度**：L1

**参考答案**：

```
① VARCHAR 最大长度
   VARCHAR(255) 和 VARCHAR(256) 的 key_len 不同：
     小于 255：1 字节前缀长度
     大于等于 256：2 字节前缀长度
   → 控制长度在 255 以内，索引扫描更高效

② DECIMAL vs FLOAT
   DECIMAL(10,2) 精确小数 → 金钱相关必须用 DECIMAL
   FLOAT/DOUBLE 有精度误差 → 不要用于金额比较

③ TIMESTAMP vs DATETIME
   TIMESTAMP：4 字节，范围 1970-2038，受时区影响
   DATETIME：8 字节（5.6+），范围 1000-9999，不受时区影响
   → 新系统默认用 DATETIME(6)（支持微秒）

④ CHAR vs VARCHAR
   CHAR 固定长度，VARCHAR 变长
   → CHAR 适合定长数据（MD5、手机号），VARCHAR 适合不定长（用户名、地址）

⑤ 数值类型的选择
   TINYINT(1)（1 字节）vs INT(11)（4 字节）
   → 状态/枚举值用 TINYINT，不要全部用 INT
```

---

### Q32：慢查询日志和分析工具的使用？

**难度**：L1

**参考答案**：

**开启和配置**：

```sql
-- 查看状态
SHOW VARIABLES LIKE 'slow_query%';
SHOW VARIABLES LIKE 'long_query_time';

-- 开启慢查询日志
SET GLOBAL slow_query_log = ON;
SET GLOBAL long_query_time = 1;       -- 单位：秒，建议设为 1
SET GLOBAL log_queries_not_using_indexes = ON;  -- 记录没用到索引的查询
```

**分析工具**：

```bash
# mysqldumpslow - 提取 SQL 摘要
mysqldumpslow -s t -t 10 /var/log/mysql/slow.log
# -s t：按查询时间排序  -t 10：取 Top 10

# pt-query-digest（Percona Toolkit）
pt-query-digest /var/log/mysql/slow.log
# 输出：Query Response Time histogram + Top N 慢查询摘要
```

---

### Q33：JOIN 查询在数据量大时怎么优化？大表 JOIN 大表怎么办？

**难度**：L2

**参考答案**：

**核心优化方向**：

```
① 确保 JOIN 条件字段有索引
   被驱动表的关联列必须有索引 → Nested Loop Join 效率倍增

② 小表驱动大表
   LEFT JOIN：左边是驱动表，尽量让小的表在左边（但优化器可能重排）

③ 减少返回列
   只 SELECT 需要的列，不要 SELECT *（减少回表和网络传输）

④ 分批查询
   一次性 JOIN 100 万条，不如拆成 1000 次 × 1000 条（适合离线任务）

⑤ 考虑反范式化
   高频 JOIN 的字段冗余进主表（如 order 冗余 user_name）
```

**大表 JOIN 大表**方案（几百万以上 JOIN 几百万以上）：

```
① 汇总表/物化视图（MySQL 8.0 的 Generated Column + Index）
   定期预计算 JOIN 结果，存为汇总表

② Elasticsearch 异构
   JOIN 查询的条件写入 ES，查出的主键 ID 再去 DB 拉取详情

③ 应用层内存 JOIN
   分别查两张表的数据，应用层做 hash join（数据量可控时）

④ 分库分表 + 全局表
   字典表（如商品、用户）全库复制，避免跨分片 JOIN
```

---

### Q34：连接池应该设置多大？为什么 "连接数越多越好" 是错的？

**难度**：L2

**参考答案**：

**"多不一定好"的原因**：

```
过多连接带来的问题：
① 上下文切换开销
   CPU 核数有限 → 大量连接争抢 CPU → 上下文切换增加

② 锁竞争加剧
   InnoDB 的行锁、Buffer Pool 的 mutex、redo log 的 mutex
   连接越多 → 锁等待越严重 → 吞吐量反而下降

③ 内存消耗
   每个连接：thread_stack（默认 256KB） + net_buffer（默认 16KB） + ...
   500 个连接约消耗 200+ MB 内存
```

**推荐公式**（PostgreSQL 官方文档提出，MySQL 同样适用）：

```
连接数 = ((核心数 × 2) + 有效磁盘数)

举例：4 核 CPU + 1 块 SSD → 连接数 = 8 + 1 = 9
      16 核 CPU + RAID 10（4 磁盘）→ 连接数 = 32 + 4 = 36
```

**实际建议**：
- **OLTP 系统**：一般设置 **50-200** 足够
- **连接数超过 200** 时，优先考虑增加**连接池排队机制**（如 HikariCP），而不是增加连接
- 超过 300 后基本靠**连接复用**而非并行处理，再多只会降低性能

**常用连接池配置**：

```yaml
# HikariCP（Spring Boot 默认）
maximum-pool-size: 20       # 最大连接数（不是越大越好）
minimum-idle: 5              # 最小空闲连接
connection-timeout: 5000     # 获取连接超时
idle-timeout: 600000         # 空闲超时
```

---

### Q35：MySQL 版本演进（5.6 → 5.7 → 8.0）有哪些关键变化？

**难度**：L2

**参考答案**：

| 版本 | 关键特性 | 意义 |
|---|---|---|
| **5.6** | GTID 复制、ICP、Online DDL（部分） | 在线操作能力大幅提升 |
| **5.7** | JSON 类型、性能 Schema 增强、Group Replication（MGR） | 半结构化数据支持，高可用 |
| **8.0** | Window Function、CTE、Hash Join、Instant DDL、事务字典、直方图、降序索引、不可见索引 | 分析查询能力、DDL 零阻塞 |

**升级注意事项**（5.7 → 8.0）：
1. **字符集默认从 latin1 变为 utf8mb4** → 检查默认字符集依赖
2. **GROUP BY 不再隐式排序** → `ORDER BY NULL` 不再需要，但如果有依赖排序的应用会受影响
3. **密码插件变更** → `mysql_native_password` → `caching_sha2_password`，旧客户端连不上
4. **数据字典从 .frm 文件变为系统表** → `INFORMATION_SCHEMA` 查询更快

---

## 附：面试官评分标准

| 等级 | 能力画像 | 典型表现 |
|---|---|---|
| **L1** | 能用 SQL | 会 SELECT/JOIN/索引，理解事务概念，能通过 EXPLAIN 看执行计划 |
| **L2** | 会调优 | 能分析慢查询根源，理解 MVCC/锁/隔离级别，能设计分库分表 |
| **L3** | 懂原理 | 读过 InnoDB 源码级机制（Redo Log 格式、B+ 树分裂策略），能设计高可用架构 |
| **L4** | 能搭系统 | 理解分布式一致协议（Raft/Paxos），能设计大规模分布式数据库或 Sharding 方案 |

> **编者注**：项目中包含 Redis 核心数据结构的手写实现（`engineering/src/redis/`）、SQLite 笔记（`learning/notes/sqlite/`）、
> 以及数据库面试基础问答（`learning/interview/database/questions.md`），可作为面试准备参考。
