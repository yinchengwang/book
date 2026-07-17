# B+树实现与优化

## 概述
演示 B+树实现的核心工程细节：页内结构、页分裂合并、索引下推(ICP)、MRR。

## 编译运行
make && ./test.exe

## 核心概念
- Page Directory 槽定位（二分查找）
- 页分裂与合并
- 索引下推 (Index Condition Pushdown)
- MRR (Multi-Range Read) 批量回表

## MySQL InnoDB 对照
- InnoDB 页面大小 16KB，Page Directory 槽指向记录偏移
- 分裂时机：页满时自顶向下预分裂，避免递归上浮
- 合并时机：删除后低于半满，与相邻页合并