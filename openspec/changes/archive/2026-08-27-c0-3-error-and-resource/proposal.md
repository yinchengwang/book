# C0-3 统一错误码与资源管理 Proposal

## Why

差距分析（README §3.3）发现错误码各模态私有（KV 7 值枚举 vs Vector 返回 -1/0 vs SQL 层另一体系），跨层无法统一判断；执行器初始化错误路径手工逐资源 free（`nodeSeqscan.c:160-176`）易漏；`mm_storage` 契约存在假成功（vector drop 空操作返回 0，`vector_engine.c:351-354`；scan 恒 NULL，`:435-444`）；mm_insert 数据格式手工字节偏移无版本（`vector_engine.c:360-372`）。

## What Changes

- `include/db/errors.h` 扩展统一 `DBERR_*` 空间（通用类 + 模态前缀映射），各模态错误码映射进统一空间
- 执行器 PlanState/EState 创建挂 per-query MemoryContext，EndPlan 统一 Reset（替代手工 free 链）
- `mm_storage` 契约补全：未实现操作返回 `DBERR_NOT_IMPLEMENTED`（vector drop/scan 首批修正）
- mm_insert 序列化契约：`mm_record_header_t`（magic + version + model + len），各模态解析统一走头部

## Capabilities

| 能力 | 交付 |
|------|------|
| 统一错误码 | DBERR_* 全量定义 + 各模态映射表 + 单元测试 |
| 资源清理 | 执行器初始化路径 MemoryContext 化，nodeSeqscan 手工 free 链删除 |
| 契约完整 | mm_storage 全接口真实实现或 DBERR_NOT_IMPLEMENTED |
| 序列化版本 | mm_record_header_t + 新旧格式兼容读取 |

## Impact

- 修改：errors.h、memctx.c、executor.c、各 node*.c 初始化路径、mm_storage.h、各引擎 API 层
- 预计 6-8 个 commit
