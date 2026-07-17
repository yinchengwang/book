## ADDED Requirements

### Requirement: pop_back 通过容器分配器销毁尾元素
对于非空的 `mystl::vector<T, Alloc>`，系统 SHALL 先将结束指针递减到原尾元素位置，再调用该 vector 持有的分配器实例的 `destroy` 销毁该位置的元素。

#### Scenario: 多元素容器弹出尾元素
- **WHEN** 一个包含多个元素的 vector 调用 `pop_back`
- **THEN** 分配器的 `destroy` 恰好收到原尾元素地址，且 vector 的大小减少一并保留此前元素

#### Scenario: 单元素容器弹出到空
- **WHEN** 一个仅包含一个元素的 vector 调用 `pop_back`
- **THEN** 分配器销毁唯一元素，且 vector 变为空容器

### Requirement: pop_back 边界回归测试
测试套件 MUST 覆盖多元素尾部弹出和单元素弹出到空两个非空边界，并验证分配器销毁钩子的调用次数与目标地址。

#### Scenario: 修复前测试能够捕获直接析构
- **WHEN** `pop_back` 绕过分配器并直接调用元素析构函数
- **THEN** 回归测试因分配器 `destroy` 调用次数不符合预期而失败
