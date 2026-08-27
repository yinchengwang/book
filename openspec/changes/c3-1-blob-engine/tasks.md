# C3-1 对象 Blob 存储引擎（自研）任务清单

## 任务列表

- [ ] **T1** 分块布局设计（4MB chunk + SHA-256 内容寻址）
- [ ] **T2** chunk 存储（本地文件 + LRU 缓存挂 Buffer Pool）
- [ ] **T3** 元数据走 KV catalog（blob-id → chunk 列表 + 元信息）
- [ ] **T4** blob_put/get/delete/stat 基础 API
- [ ] **T5** blob_range_get（按 offset/len 范围读）
- [ ] **T6** Multipart API（begin/upload/complete/abort）
- [ ] **T7** 内容去重（同 SHA-256 增量引用计数）
- [ ] **T8** 持久化集成（chunk 文件 fsync，元数据走 KV WAL）
- [ ] **T9** 大文件集成测试（GB 级 put + range get + 重启恢复）
- [ ] **T10** mm_storage 接入（BLOB 类型 + 与 TOAST 接口预留）
- [ ] **T11** Verify + Archive
