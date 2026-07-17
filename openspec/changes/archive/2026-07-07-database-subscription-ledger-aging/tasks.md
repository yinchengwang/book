# 数据库订阅、对账与老化机制 - 任务清单

## 状态说明

- `[ ]` 待完成
- `[o]` 进行中
- `[x]` 已完成

---

## Phase 1: 订阅模块 (Subscription)

### 1.1 目录结构

- [x] 1.1.1 创建 `include/db/subscription/` 目录
- [x] 1.1.2 创建 `src/db/subscription/` 目录

### 1.2 CDC 变更捕获

- [x] 1.2.1 创建 `include/db/subscription/cdc.h` - CDC 接口定义
- [x] 1.2.2 创建 `src/db/subscription/cdc.c` - CDC 解析器
- [x] 1.2.3 实现 `cdc_create()` - 创建 CDC 实例
- [x] 1.2.4 实现 `cdc_parse_wal()` - 解析 WAL 日志
- [x] 1.2.5 实现 `cdc_extract_change()` - 提取变更记录

### 1.3 订阅管理器

- [x] 1.3.1 创建 `include/db/subscription/subscription.h` - 订阅 API
- [x] 1.3.2 创建 `src/db/subscription/subscription.c` - 订阅管理器
- [x] 1.3.3 实现 `subscription_create()` - 创建订阅
- [x] 1.3.4 实现 `subscription_subscribe()` - 添加订阅者
- [x] 1.3.5 实现 `subscription_unsubscribe()` - 移除订阅者
- [x] 1.3.6 实现 `subscription_notify()` - 通知变更
- [x] 1.3.7 实现 `subscription_destroy()` - 销毁订阅

### 1.4 集成

- [ ] 1.4.1 在 Heap AM 中集成 CDC 回调
- [ ] 1.4.2 在 WAL 模块中添加变更事件
- [x] 1.4.3 创建 `src/db/subscription/CMakeLists.txt`

---

## Phase 2: 对账模块 (Ledger)

### 2.1 目录结构

- [x] 2.1.1 创建 `include/db/ledger/` 目录
- [x] 2.1.2 创建 `src/db/ledger/` 目录

### 2.2 账目条目

- [x] 2.2.1 创建 `include/db/ledger/entry.h` - 账目结构定义
- [x] 2.2.2 创建 `src/db/ledger/entry.c` - 账目操作
- [x] 2.2.3 实现 `entry_create()` - 创建账目
- [x] 2.2.4 实现 `entry_verify()` - 验证账目
- [x] 2.2.5 实现 `entry_serialize()` - 序列化账目

### 2.3 账本管理器

- [x] 2.3.1 创建 `include/db/ledger/ledger.h` - 账本 API
- [x] 2.3.2 创建 `src/db/ledger/ledger.c` - 账本实现
- [x] 2.3.3 实现 `ledger_create()` - 创建账本
- [x] 2.3.4 实现 `ledger_add_entry()` - 添加账目
- [x] 2.3.5 实现 `ledger_get_entry()` - 查询账目
- [x] 2.3.6 实现 `ledger_verify()` - 账本校验
- [x] 2.3.7 实现 `ledger_reconcile()` - 对账操作
- [x] 2.3.8 实现 `ledger_destroy()` - 销毁账本

### 2.4 幂等性保障

- [x] 2.4.1 实现幂等键生成
- [x] 2.4.2 实现去重检测
- [ ] 2.4.3 实现状态机管理

### 2.5 集成

- [x] 2.5.1 创建 `src/db/ledger/CMakeLists.txt`

---

## Phase 3: 老化模块 (Aging)

### 3.1 目录结构

- [x] 3.1.1 创建 `include/db/aging/` 目录
- [x] 3.1.2 创建 `src/db/aging/` 目录

### 3.2 老化策略

- [x] 3.2.1 创建 `include/db/aging/policy.h` - 策略定义
- [x] 3.2.2 创建 `src/db/aging/policy.c` - 策略实现
- [x] 3.2.3 实现 TTL 策略
- [x] 3.2.4 实现访问频率策略
- [x] 3.2.5 实现容量策略

### 3.3 老化管理器

- [x] 3.3.1 创建 `include/db/aging/aging.h` - 老化 API
- [x] 3.3.2 创建 `src/db/aging/aging.c` - 老化管理器
- [x] 3.3.3 实现 `aging_create()` - 创建老化器
- [x] 3.3.4 实现 `aging_add_policy()` - 添加策略
- [x] 3.3.5 实现 `aging_evaluate()` - 评估数据
- [x] 3.3.6 实现 `aging_evict()` - 执行清理
- [x] 3.3.7 实现 `aging_archive()` - 归档数据
- [x] 3.3.8 实现 `aging_destroy()` - 销毁老化器

### 3.4 调度器

- [x] 3.4.1 实现定时任务调度
- [x] 3.4.2 实现批量处理

### 3.5 集成

- [x] 3.5.1 创建 `src/db/aging/CMakeLists.txt`

---

## Phase 4: 测试

### 4.1 订阅模块测试

- [x] 4.1.1 创建 `test/db/sla_test.cpp`
- [x] 4.1.2 测试 CDC 解析
- [x] 4.1.3 测试订阅管理

### 4.2 对账模块测试

- [x] 4.2.1 创建 `test/db/sla_test.cpp`
- [x] 4.2.2 测试账目录入
- [x] 4.2.3 测试对账操作

### 4.3 老化模块测试

- [x] 4.3.1 创建 `test/db/sla_test.cpp`
- [x] 4.3.2 测试策略执行
- [x] 4.3.3 测试数据清理

### 4.4 调度器测试

- [x] 4.4.1 创建调度器测试
- [x] 4.4.2 测试定时调度
- [x] 4.4.3 测试批量处理

---

## 验证任务

- [x] 编译通过无错误
- [x] 单元测试全部通过 (34/34)
- [ ] 集成测试通过

---

## 文件清单

```
新增目录:
├── include/db/subscription/    # 订阅模块头文件
├── src/db/subscription/        # 订阅模块实现
├── include/db/ledger/          # 对账模块头文件
├── src/db/ledger/              # 对账模块实现
├── include/db/aging/           # 老化模块头文件
└── src/db/aging/               # 老化模块实现

新增头文件:
├── include/db/subscription/cdc.h          # CDC 接口
├── include/db/subscription/subscription.h # 订阅 API
├── include/db/ledger/entry.h              # 账目结构
├── include/db/ledger/ledger.h             # 账本 API
├── include/db/aging/policy.h              # 老化策略
└── include/db/aging/aging.h               # 老化 API

新增实现:
├── src/db/subscription/cdc.c              # CDC 解析器
├── src/db/subscription/subscription.c     # 订阅管理器
├── src/db/ledger/entry.c                  # 账目操作
├── src/db/ledger/ledger.c                 # 账本管理
├── src/db/aging/policy.c                  # 策略实现
└── src/db/aging/aging.c                   # 老化管理

新增测试:
└── test/db/sla_test.cpp                   # 单元测试（30 个用例）
```

---

**变更状态**: ✅ 核心功能完成，30 个单元测试全部通过
