# C7 内核与索引 任务清单

## 任务列表（已实装标记）

### 索引核心（C7.1-C7.3）
- [x] **C7.1** Hash 索引 htable（bucket page + 哈希桶）—— 骨架已落
- [x] **C7.2** TOAST 大元组外存（heap_insert 自动判断 + blob_id 引用）—— 骨架
- [x] **C7.3** FSM bitmap（每页 1 bit 空闲状态）—— 骨架

### KV 增强（C7.4-C7.5）
- [x] **C7.4** kv_txn 乐观事务
- [x] **C7.5** 跨 CF WriteBatch

### Tree 与宽表（C7.6-C7.7）
- [x] **C7.6** ltree 路径列类型与操作符骨架
- [x] **C7.7** wide_row 范围扫描完整实装（prefix scan）

### 收尾（C7.8）
- [x] **C7.8** 集成测试 + Verify + Archive
