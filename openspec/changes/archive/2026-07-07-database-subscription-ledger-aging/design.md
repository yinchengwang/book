# 数据库订阅、对账与老化机制 - 设计文档

## 架构概览

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           数据库核心层                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                          │
│  │    Heap     │  │   BTree     │  │   WAL       │                          │
│  │   Storage   │  │   Index     │  │   Log       │                          │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘                          │
│         │                │                │                                 │
│         └────────────────┼────────────────┘                                 │
│                          │                                                  │
│         ┌────────────────▼────────────────┐                                 │
│         │         CDC 变更捕获层           │                                 │
│         │    (Change Data Capture)        │                                 │
│         └────────────────┬────────────────┘                                 │
└──────────────────────────┼──────────────────────────────────────────────────┘
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
        ▼                  ▼                  ▼
┌───────────────┐  ┌───────────────┐  ┌───────────────┐
│   订阅模块     │  │   对账模块     │  │   老化模块     │
│ Subscription  │  │    Ledger     │  │    Aging      │
└───────────────┘  └───────────────┘  └───────────────┘
```

---

## 1. 订阅模块设计

### 1.1 核心概念

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              订阅模型                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   数据库 ──[变更]──▶ CDC ──[解析]──▶ 订阅管理器 ──[分发]──▶ 多个订阅者       │
│                                         │                                   │
│                              ┌──────────┼──────────┐                         │
│                              │          │          │                         │
│                         [订阅者1]  [订阅者2]  [订阅者3]                       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 CDC 变更类型

```c
/**
 * @brief CDC 变更类型枚举
 */
typedef enum {
    CDC_CHANGE_INSERT = 0,   /**< 插入操作 */
    CDC_CHANGE_UPDATE = 1,   /**< 更新操作 */
    CDC_CHANGE_DELETE = 2,   /**< 删除操作 */
    CDC_CHANGE_TRUNCATE = 3, /**< 截断操作 */
} cdc_change_type_t;

/**
 * @brief CDC 变更记录
 */
typedef struct cdc_change_s {
    uint64_t lsn;                /**< WAL 日志序列号 */
    cdc_change_type_t type;      /**< 变更类型 */
    int32_t relation_id;         /**< 关系 ID */
    uint64_t transaction_id;     /**< 事务 ID */
    uint64_t timestamp;          /**< 变更时间戳 */
    void *old_tuple;             /**< 旧元组（UPDATE/DELETE） */
    void *new_tuple;             /**< 新元组（INSERT/UPDATE） */
} cdc_change_t;
```

### 1.3 订阅结构

```c
/**
 * @brief 订阅者回调函数类型
 */
typedef int (*subscription_callback_t)(const cdc_change_t *change, void *user_data);

/**
 * @brief 订阅信息
 */
typedef struct subscription_s {
    char name[64];                           /**< 订阅名称 */
    int32_t *relation_ids;                   /**< 订阅的表 ID 数组 */
    int32_t relation_count;                  /**< 表数量 */
    subscription_callback_t callback;        /**< 回调函数 */
    void *user_data;                         /**< 用户数据 */
    uint64_t start_lsn;                      /**< 起始 LSN */
    uint64_t last_lsn;                       /**< 最后确认 LSN */
} subscription_t;

/**
 * @brief 订阅管理器
 */
typedef struct subscription_manager_s {
    char data_dir[512];                      /**< 数据目录 */
    subscription_t **subscriptions;          /**< 订阅数组 */
    int32_t subscription_count;              /**< 订阅数量 */
    int32_t capacity;                        /**< 容量 */
    pthread_mutex_t mutex;                   /**< 互斥锁 */
} subscription_manager_t;
```

---

## 2. 对账模块设计

### 2.1 核心概念

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              账本模型                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   账本 (Ledger)                                                            │
│   ├── entries[]          账目数组                                            │
│   ├── current_balance    当前余额                                           │
│   ├── last_sequence      最后序列号                                         │
│   └── checksum           校验和                                             │
│                                                                             │
│   账目录入格式:                                                             │
│   ┌─────────────────────────────────────────────────────────────────┐       │
│   │ sequence | idempotency_key | debit | credit | balance | hash   │       │
│   │    1     |  txn_123_abc    |  100  |    0   |   100   | abc... │       │
│   │    2     |  txn_456_def    |    0  |   50   |    50   | def... │       │
│   └─────────────────────────────────────────────────────────────────┘       │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 账目结构

```c
/**
 * @brief 账目录入类型
 */
typedef enum {
    LEDGER_ENTRY_DEBIT = 0,   /**< 借方（支出） */
    LEDGER_ENTRY_CREDIT = 1,  /**< 贷方（收入） */
} ledger_entry_type_t;

/**
 * @brief 账目条目
 */
typedef struct ledger_entry_s {
    uint64_t sequence;              /**< 序列号（递增） */
    char idempotency_key[128];      /**< 幂等键 */
    ledger_entry_type_t type;       /**< 类型 */
    int64_t amount;                 /**< 金额（分） */
    int64_t balance;                /**< 余额 */
    char description[256];          /**< 描述 */
    uint64_t timestamp;             /**< 时间戳 */
    uint8_t hash[32];               /**< SHA-256 哈希 */
} ledger_entry_t;

/**
 * @brief 账本
 */
typedef struct ledger_s {
    char name[64];                  /**< 账本名称 */
    char data_dir[512];             /**< 数据目录 */
    int64_t current_balance;        /**< 当前余额 */
    uint64_t last_sequence;         /**< 最后序列号 */
    ledger_entry_t **entries;       /**< 账目数组 */
    int32_t entry_count;            /**< 账目数量 */
    int32_t capacity;               /**< 容量 */
    pthread_mutex_t mutex;          /**< 互斥锁 */
} ledger_t;
```

### 2.3 对账操作

```c
/**
 * @brief 对账结果
 */
typedef struct ledger_reconcile_result_s {
    bool is_balanced;               /**< 是否平衡 */
    int64_t expected_balance;       /**< 期望余额 */
    int64_t actual_balance;         /**< 实际余额 */
    int64_t discrepancy;            /**< 差异金额 */
    int32_t missing_entries;        /**< 缺失条目数 */
} ledger_reconcile_result_t;

/**
 * @brief 对账流程
 *
 * 1. 计算所有借方总额 - 所有贷方总额 = 余额
 * 2. 验证余额与 current_balance 一致
 * 3. 验证每条记录的哈希链完整性
 * 4. 检查幂等键重复
 */
```

---

## 3. 老化模块设计

### 3.1 核心概念

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              老化分层模型                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   热数据层 (Hot)     ◄─── 最近访问的数据，保留在内存                          │
│         │                                                             │
│         ▼ 访问减少 / 时间过期                                             │
│   温数据层 (Warm)    ◄─── 访问频率降低，可移到慢存储                         │
│         │                                                             │
│         ▼ 长时间未访问                                                   │
│   冷数据层 (Cold)    ◄─── 历史数据，归档存储                                │
│         │                                                             │
│         ▼ 超过保留期                                                     │
│   清理删除                                                                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 老化策略

```c
/**
 * @brief 老化策略类型
 */
typedef enum {
    AGING_POLICY_TTL = 0,           /**< 基于时间的 TTL 策略 */
    AGING_POLICY_ACCESS_FREQ = 1,   /**< 基于访问频率的策略 */
    AGING_POLICY_SIZE = 2,          /**< 基于容量的策略 */
    AGING_POLICY_CUSTOM = 3,        /**< 自定义策略 */
} aging_policy_type_t;

/**
 * @brief 老化策略配置
 */
typedef struct aging_policy_config_s {
    aging_policy_type_t type;       /**< 策略类型 */
    union {
        struct {
            uint64_t max_age_seconds;  /**< 最大存活时间 */
        } ttl;
        struct {
            int32_t max_access_count;  /**< 最大访问次数 */
            uint64_t time_window;      /**< 时间窗口 */
        } access_freq;
        struct {
            int64_t max_size_bytes;    /**< 最大容量 */
            float evict_ratio;         /**< 淘汰比例 */
        } size;
    } config;
} aging_policy_config_t;

/**
 * @brief 数据热度评估
 */
typedef struct data_temperature_s {
    uint64_t last_access_time;      /**< 最后访问时间 */
    int32_t access_count;           /**< 访问次数 */
    int32_t tier;                   /**< 当前层级 (0=热,1=温,2=冷) */
    float score;                    /**< 热度评分 (0-1) */
} data_temperature_t;
```

### 3.3 老化管理器

```c
/**
 * @brief 老化管理器
 */
typedef struct aging_manager_s {
    char data_dir[512];              /**< 数据目录 */
    aging_policy_config_t *policies; /**< 策略数组 */
    int32_t policy_count;            /**< 策略数量 */
    int32_t hot_tier_capacity;       /**< 热数据容量 */
    int32_t warm_tier_capacity;      /**< 温数据容量 */
    uint64_t schedule_interval;      /**< 调度间隔（秒） */
    pthread_mutex_t mutex;           /**< 互斥锁 */
} aging_manager_t;

/**
 * @brief 老化评估结果
 */
typedef enum {
    AGING_KEEP = 0,                 /**< 保留 */
    AGING_MOVE_WARM = 1,            /**< 移至温层 */
    AGING_MOVE_COLD = 2,            /**< 移至冷层 */
    AGING_ARCHIVE = 3,              /**< 归档 */
    AGING_DELETE = 4,               /**< 删除 */
} aging_action_t;
```

---

## 4. 集成设计

### 4.1 与现有模块的集成

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              模块集成关系                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   Heap AM                                                                   │
│   ├── insert_tuple() ────► CDC.on_insert() ────► Subscription.notify()      │
│   ├── update_tuple() ────► CDC.on_update()                                  │
│   └── delete_tuple() ────► CDC.on_delete()                                  │
│                                                                             │
│   Buffer Pool                                                                │
│   └── page_evict() ───────► Aging.evaluate() ────► Aging.action()           │
│                                                                             │
│   WAL                                                                        │
│   └── checkpoint() ───────► Ledger.checkpoint()                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 头文件依赖

```
include/db/
├── subscription/
│   ├── cdc.h              # CDC 接口（被 heapam.h 包含）
│   └── subscription.h     # 订阅接口
├── ledger/
│   ├── entry.h            # 账目结构
│   └── ledger.h           # 账本接口
└── aging/
    ├── policy.h           # 老化策略
    └── aging.h            # 老化接口
```

---

## 5. API 概要

### 5.1 订阅 API

```c
subscription_manager_t *subscription_manager_create(const char *data_dir);
int subscription_subscribe(subscription_manager_t *mgr, subscription_t *sub);
int subscription_unsubscribe(subscription_manager_t *mgr, const char *name);
int subscription_notify(subscription_manager_t *mgr, const cdc_change_t *change);
void subscription_manager_destroy(subscription_manager_t *mgr);
```

### 5.2 对账 API

```c
ledger_t *ledger_create(const char *name, const char *data_dir);
int ledger_add_entry(ledger_t *ledger, const ledger_entry_t *entry);
ledger_entry_t *ledger_get_entry(ledger_t *ledger, uint64_t sequence);
int ledger_verify(ledger_t *ledger);
ledger_reconcile_result_t *ledger_reconcile(ledger_t *ledger);
void ledger_destroy(ledger_t *ledger);
```

### 5.3 老化 API

```c
aging_manager_t *aging_manager_create(const char *data_dir);
int aging_add_policy(aging_manager_t *mgr, const aging_policy_config_t *policy);
aging_action_t aging_evaluate(aging_manager_t *mgr, const data_temperature_t *temp);
int aging_evict(aging_manager_t *mgr);
int aging_archive(aging_manager_t *mgr, const void *data, size_t len);
void aging_manager_destroy(aging_manager_t *mgr);
```

---

## 6. 实现优先级

| 优先级 | 模块 | 原因 |
|--------|------|------|
| P0 | 订阅 CDC 框架 | 基础设施，其他模块依赖 |
| P0 | 老化策略框架 | 存储优化急需 |
| P1 | 订阅管理器 | 完整订阅功能 |
| P1 | 对账核心 | 账务一致性需求 |
| P2 | 幂等性保障 | 对账增强 |
| P2 | 老化调度器 | 自动化运维 |
