# C3-1 对象 Blob 存储引擎（自研）任务清单

## 任务列表

- [x] **T1** 分块布局设计（4MB chunk + SHA-256 内容寻址）
- [x] **T2** chunk 存储（本地文件 + stat 去重）
- [ ] **T3** 元数据走 KV catalog（推迟：当前直接路径模式，完整 catalog 留后续）
- [x] **T4** blob_put/get/delete/stat 基础 API
- [x] **T5** blob_range_get（按 offset/len 范围读）
- [ ] **T6** Multipart API（推迟：复杂度高，单变更控制）
- [x] **T7** 内容去重（同 SHA-256 + stat 检测）
- [x] **T8** 持久化集成（chunk 文件 fsync 跨平台）
- [ ] **T9** 大文件集成测试（推迟）
- [ ] **T10** mm_storage 接入（推迟）
- [x] **T11** Verify + Archive
