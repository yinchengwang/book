# apps-web-knowledge-hub

学习追踪平台规范，Taro + React 技术栈（H5 + 微信小程序双端）。

## ADDED Requirements

### Requirement: 页面结构
系统 SHALL 提供分类选择页和详情页两种主要页面。

#### Scenario: 分类选择页
- **WHEN** 用户打开知识 hub 首页
- **THEN** 显示分类卡片列表（如：编程、数据库、LLM 等）

#### Scenario: 分类详情页
- **WHEN** 用户点击某个分类卡片
- **THEN** 显示该分类下的文章/笔记列表

### Requirement: 笔记管理
系统 SHALL 提供笔记的增删改查功能。

#### Scenario: 添加笔记
- **WHEN** 用户点击添加按钮并填写表单
- **THEN** 笔记被保存到本地存储

#### Scenario: 编辑笔记
- **WHEN** 用户点击笔记并编辑
- **THEN** 笔记更新并保存

#### Scenario: 删除笔记
- **WHEN** 用户滑动删除或点击删除按钮
- **THEN** 笔记从列表中移除

### Requirement: 数据持久化
系统 SHALL 使用本地存储保存用户数据。

#### Scenario: 数据保存
- **WHEN** 用户添加或编辑笔记
- **THEN** 数据自动保存到 localStorage

#### Scenario: 数据恢复
- **WHEN** 用户重新打开应用
- **THEN** 从 localStorage 恢复之前的数据

### Requirement: 双端适配
系统 SHALL 同时支持 H5 浏览器和微信小程序平台。

#### Scenario: H5 平台运行
- **WHEN** 在浏览器中打开应用
- **THEN** 所有功能正常工作

#### Scenario: 微信小程序运行
- **WHEN** 在微信开发者工具中打开
- **THEN** 所有功能正常工作