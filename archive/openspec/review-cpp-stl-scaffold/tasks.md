## 1. vector::pop_back 修复

- [x] 1.1 创建独立的 `vector_pop_back_test.cpp` 并增加自定义计数分配器测试夹具
- [x] 1.2 添加多元素 `pop_back` 的分配器销毁地址与调用次数回归测试，并确认修复前失败
- [x] 1.3 添加单元素 `pop_back` 到空容器的边界测试，并确认修复前失败
- [x] 1.4 将 `pop_back` 改为递减 `finish_` 后调用 `alloc_.destroy(finish_)`
- [ ] 1.5 运行相关测试并确认所有场景通过

## 2. 交付

- [ ] 2.1 检查 proposal、design 与规格覆盖最终实现范围
- [ ] 2.2 精确暂存并提交仅与本任务相关的代码、测试和 OpenSpec 工件
- [ ] 2.3 将完整执行结果写入 `.superpowers/sdd/task-1-5-report.md`
