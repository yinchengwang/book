# 文档倒排索引规范

## ADDED Requirements

### Requirement: 倒排索引存储

文档存储引擎 SHALL 实现倒排索引以支持高效全文搜索。

#### Scenario: 创建倒排索引
- **WHEN** 调用 `doc_inverted_index_create(path)`
- **THEN** 系统 SHALL 创建术语字典和倒排列表存储
- **AND** 系统 SHALL 初始化文档存储
- **AND** 系统 SHALL 返回索引句柄

#### Scenario: 索引文档
- **WHEN** 调用 `doc_inverted_index_add(index, doc_id, doc_data)`
- **THEN** 系统 SHALL 解析文档并分词
- **AND** 系统 SHALL 为每个词项更新倒排列表
- **AND** 系统 SHALL 存储文档数据

#### Scenario: 移除文档
- **WHEN** 调用 `doc_inverted_index_remove(index, doc_id)`
- **THEN** 系统 SHALL 从所有相关倒排列表中标记墓碑
- **AND** 系统 SHALL 从文档存储中删除文档
- **AND** 系统 SHALL 定期合并清理墓碑

### Requirement: 术语字典

倒排索引 SHALL 使用 FST (Finite State Transducer) 作为术语字典。

#### Scenario: FST 术语查找
- **WHEN** 查找术语时
- **THEN** 系统 SHALL 使用 FST 导航到术语节点
- **AND** 系统 SHALL 返回术语信息和倒排列表指针
- **AND** 查找时间 SHALL 为 O(term_length)

#### Scenario: 前缀查询
- **WHEN** 执行前缀查询 "hel*" 时
- **THEN** 系统 SHALL 使用 FST 遍历匹配前缀的状态
- **AND** 系统 SHALL 返回所有匹配术语

### Requirement: 倒排列表存储

倒排列表 SHALL 使用增量编码压缩。

#### Scenario: 添加到倒排列表
- **WHEN** 新文档包含术语 T 时
- **THEN** 系统 SHALL 计算 doc_id 与上一文档的差值
- **AND** 系统 SHALL 使用可变长编码存储差值
- **AND** 系统 SHALL 存储词频和位置信息

#### Scenario: 压缩倒排列表
- **WHEN** 倒排列表达到阈值时
- **THEN** 系统 SHALL 应用更高压缩率的重编码
- **AND** 系统 SHALL 更新存储大小

### Requirement: 倒排索引文件格式 SHALL 遵循标准目录结构

```
inverted_index/
  ├── dict.fst          # FST 术语字典
  ├── postings/
  │   ├── posting_0000.bin  # 倒排列表分片
  │   ├── posting_0001.bin
  │   └── ...
  ├── docs/
  │   └── docs.bin      # 文档存储
  └── meta.bin          # 索引元数据

meta.bin:
  - magic, version, doc_count, term_count
  - segment_count
  - last_merge_time
```

#### Scenario: 保存倒排索引
- **WHEN** 调用 `doc_inverted_index_save(index)`
- **THEN** 系统 SHALL 保存 FST 字典
- **AND** 系统 SHALL 保存所有倒排列表
- **AND** 系统 SHALL 保存文档存储

#### Scenario: 加载倒排索引
- **WHEN** 调用 `doc_inverted_index_load(path)`
- **THEN** 系统 SHALL 加载 FST 字典
- **AND** 系统 SHALL 加载倒排列表头信息
- **AND** 系统 SHALL 按需加载倒排列表内容
