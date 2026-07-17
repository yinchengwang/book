# 图解系列

## Purpose

`data/learn-deep/illustrate/` 目录存放独立的图解系列文章（MySQL、Redis、网络、操作系统等），按产品/主题分类存放和浏览，独立于技术栈知识体系（c/cpp/ds/db/py/linux）。

## ADDED Requirements

### Requirement: 图解系列目录结构

`data/learn-deep/illustrate/`  SHALL 按产品/主题建立子目录，每个子目录存放对应的 `.md` 文件。目录结构为：`illustrate/{product}/{id}.md`，其中 `product` 包括 mysql、redis、network、os、agent、claude-code、faiss、diskann、milvus、pinecone、opengauss、elasticsearch、postgres、rocksdb、sqlite。

#### Scenario: MySQL 图解文件

- **WHEN** 查看 MySQL 图解内容
- **THEN** 文件 SHALL 存放在 `data/learn-deep/illustrate/mysql/` 目录下

#### Scenario: 网络图解文件

- **WHEN** 查看网络图解内容
- **THEN** 文件 SHALL 存放在 `data/learn-deep/illustrate/network/` 目录下

### Requirement: 图解系列导航

`index.html` SHALL 新增"图解系列"导航入口，列出所有已创建的图解系列目录。用户可从导航进入对应图解系列的目录页（或直接在 `learn.html` 中通过分类筛选浏览图解文章）。

#### Scenario: 图解系列导航入口

- **WHEN** 用户打开 `index.html`
- **THEN** 顶部导航栏 SHALL 显示"图解系列"链接，点击后可浏览所有图解子系列

#### Scenario: 图解系列浏览

- **WHEN** 用户选择"图解 MySQL"子系列
- **THEN** 页面 SHALL 显示该系列下的所有 MD 文件标题列表，点击后跳转到 `learn.html` 对应页面
