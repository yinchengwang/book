# C2-5 Tree 模块 XML 解析与 datastore 任务清单

## 任务列表

- [x] **T1** 自研 XML 解析器（属性 + 命名空间 + 前缀，递归下降）
- [x] **T2** 标准 XML 用例测试（属性/嵌套/命名空间）—— xml_parser_test 落地
- [ ] **T3** NETCONF hello 交互（推迟：依赖 netopeer2 客户端验证环境）
- [x] **T4** datastore 三态（running/candidate/startup）
- [x] **T5** candidate→running 原子 commit
- [x] **T6** datastore 持久化 + 重启恢复
- [x] **T7** YANG XPath 子集求值器（/a/b/c + [attr='v']）
- [ ] **T8** YANG import/include + grouping/uses 展开（推迟：parser 扩展）
- [x] **T9** AST arena 分配器
- [ ] **T10** NETCONF 1.1 chunked framing（推迟：netconf_server 扩展）
- [x] **T11** Verify + Archive
