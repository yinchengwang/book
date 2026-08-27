# C0-3 统一错误码与资源管理 任务清单

## 任务列表

- [ ] **T1** DBERR_* 错误码空间定义（通用类：NOT_IMPLEMENTED/WAL_FAILED/IO/FULL/CONFLICT/INVALID + 模态映射宏）
- [ ] **T2** KV/Vector/SQL 三套既有错误码映射进统一空间（向后兼容保留旧码）
- [ ] **T3** 执行器 EState 挂 per-query MemoryContext（create/EndPlan Reset）
- [ ] **T4** node*.c 初始化路径手工 free 链删除（nodeSeqscan.c:160-176 首批，其余算子跟进）
- [ ] **T5** vector drop/scan 假成功修正（vector_engine.c:351-354, 435-444 → DBERR_NOT_IMPLEMENTED 或实装）
- [ ] **T6** mm_record_header_t 序列化契约 + 各模态接入（旧格式兼容读取）
- [ ] **T7** Verify + Archive
