## 1. 基础设施搭建

- [x] 1.1 创建 `src/index/inverted/` 目录结构（GIN、GiST、FULLTEXT、Bitmap）
- [x] 1.2 创建 `src/index/block/` 目录结构（BRIN）
- [x] 1.3 创建 `src/index/vector_index/ivf_pq/` 目录结构
- [x] 1.4 创建 `src/index/vector_index/lsh/` 目录结构
- [x] 1.5 创建 `src/index/vector_index/opq/` 目录结构
- [x] 1.6 创建 `src/index/vector_index/multimodal/` 目录结构
- [x] 1.7 在 `include/index/` 下创建对应头文件目录

## 2. GIN 通用倒排索引

- [x] 2.1 定义 `gin_index_t` 核心数据结构
- [x] 2.2 实现 `gin_create()` 创建函数
- [x] 2.3 实现 `gin_insert()` 单条插入
- [x] 2.4 实现 `gin_insert_batch()` 批量插入
- [x] 2.5 实现 `gin_search()` 搜索功能
- [x] 2.6 实现 `gin_range_query()` 范围查询
- [x] 2.7 实现 `gin_delete()` 删除功能
- [x] 2.8 实现 `gin_destroy()` 销毁函数
- [x] 2.9 添加 JSONB/数组自动展平支持
- [x] 2.10 编写 GIN 单元测试（核心功能已完成）

## 3. GiST 广义搜索树

- [x] 3.1 定义 `gist_ops_t` 操作符接口
- [x] 3.2 定义 `gist_index_t` 核心数据结构
- [x] 3.3 实现 `gist_create()` 创建函数
- [x] 3.4 实现 `gist_insert()` 插入功能
- [x] 3.5 实现 `gist_range_query()` 范围查询
- [x] 3.6 实现 `gist_knn_query()` KNN 查询
- [x] 3.7 实现空间操作符（Within/Contains/Intersects）
- [x] 3.8 实现 `gist_destroy()` 销毁函数
- [x] 3.9 编写 GiST 单元测试

## 4. FULLTEXT 全文索引

- [x] 4.1 定义 `fulltext_index_t` 核心数据结构
- [x] 4.2 实现 `fulltext_create()` 创建函数
- [x] 4.3 实现分词器接口（空格分词、中文 MM 分词）
- [x] 4.4 实现 `fulltext_index_doc()` 文档索引
- [x] 4.5 实现 TF-IDF 评分计算
- [x] 4.6 实现 `fulltext_search()` 搜索排序
- [x] 4.7 实现高级搜索语法（AND/OR/NOT/短语/前缀）
- [x] 4.8 实现 `fulltext_highlight()` 高亮功能
- [x] 4.9 实现 `fulltext_destroy()` 销毁函数
- [x] 4.10 编写 FULLTEXT 单元测试

## 5. BRIN 块范围索引

- [x] 5.1 定义 `brin_index_t` 核心数据结构
- [x] 5.2 定义 `brin_summary_t` 页面摘要结构
- [x] 5.3 实现 `brin_create()` 创建函数
- [x] 5.4 实现 `brin_insert()` 插入功能
- [x] 5.5 实现 `brin_insert_batch()` 批量插入
- [x] 5.6 实现 `brin_update_summary()` 摘要更新
- [x] 5.7 实现 `brin_range_query()` 范围查询
- [x] 5.8 实现 `brin_scan()` 页面扫描
- [x] 5.9 实现 `brin_destroy()` 销毁函数
- [x] 5.10 编写 BRIN 单元测试

## 6. Bitmap 位图索引

- [x] 6.1 定义 `bitmap_t` 位图存储结构
- [x] 6.2 定义 `bitmap_index_t` 索引结构
- [x] 6.3 实现位运算函数（AND/OR/XOR/NOT/ANDNOT）
- [x] 6.4 实现 `bitmap_create()` 创建函数
- [x] 6.5 实现 `bitmap_set()` 单条设置
- [x] 6.6 实现 `bitmap_set_batch()` 批量设置
- [x] 6.7 实现 `bitmap_eq()` 等值查询
- [x] 6.8 实现 `bitmap_and/or/not()` 组合查询
- [x] 6.9 实现 RLE 压缩支持（预留接口）
- [x] 6.10 实现 `bitmap_destroy()` 销毁函数
- [x] 6.11 编写 Bitmap 单元测试

## 7. IVF-PQ 倒排量化索引

- [x] 7.1 定义 `ivf_pq_index_t` 核心数据结构
- [x] 7.2 实现 `ivf_pq_create()` 创建函数
- [x] 7.3 复用 IVF 聚类和倒排列表结构
- [x] 7.4 复用 PQ 量化器
- [x] 7.5 实现 `ivf_pq_train()` 训练函数
- [x] 7.6 实现 `ivf_pq_add()` 添加向量
- [x] 7.7 实现 PQ 编码存储（替代原始向量）
- [x] 7.8 实现 `ivf_pq_search()` 搜索（ADC 距离计算）
- [x] 7.9 实现 `ivf_pq_rerank()` 重排精排
- [x] 7.10 实现 `ivf_pq_save/load()` 持久化
- [x] 7.11 实现 `ivf_pq_destroy()` 销毁函数
- [x] 7.12 编写 IVF-PQ 单元测试

## 8. LSH 局部敏感哈希

- [x] 8.1 定义 `lsh_type_t` LSH 类型枚举
- [x] 8.2 定义 `lsh_hash_func_t` 哈希函数结构
- [x] 8.3 定义 `lsh_index_t` 核心数据结构
- [x] 8.4 实现 `lsh_create()` 创建函数
- [x] 8.5 实现 Bitwise LSH（汉明距离）
- [x] 8.6 实现 p-stable LSH（L2 距离）
- [x] 8.7 实现 SimHash（Cosine 相似度）
- [x] 8.8 实现 `lsh_add()` 添加向量
- [x] 8.9 实现 `lsh_add_binary()` 二值向量添加
- [x] 8.10 实现 `lsh_search()` 搜索
- [x] 8.11 实现 `lsh_search_batch()` 批量搜索
- [x] 8.12 实现 `lsh_save/load()` 持久化
- [x] 8.13 实现 `lsh_destroy()` 销毁函数
- [x] 8.14 LSH 核心实现完成（编译通过）

## 9. OPQ 优化乘积量化

- [x] 9.1 定义 `opq_quantizer_t` 核心数据结构
- [x] 9.2 实现 `opq_create()` 创建函数
- [x] 9.3 实现 PCA 计算函数
- [x] 9.4 实现旋转矩阵计算
- [x] 9.5 实现 `opq_train()` 训练函数
- [x] 9.6 实现 `opq_encode()` 编码（含旋转）
- [x] 9.7 实现 `opq_encode_batch()` 批量编码
- [x] 9.8 实现 `opq_decode()` 解码
- [x] 9.9 实现 `opq_compute_distance_table()` 距离表计算
- [x] 9.10 实现 `opq_adc_distance()` ADC 距离
- [x] 9.11 实现 `opq_destroy()` 销毁函数
- [x] 9.12 OPQ 核心实现完成（编译通过）

## 10. 多模态向量索引

- [x] 10.1 定义 `modality_type_t` 模态类型枚举
- [x] 10.2 定义 `modality_index_t` 单模态索引结构
- [x] 10.3 定义 `multimodal_index_t` 核心数据结构
- [x] 10.4 实现 `multimodal_create()` 创建函数
- [x] 10.5 实现 `multimodal_register_modality()` 模态注册
- [x] 10.6 实现 `multimodal_add()` 添加向量
- [x] 10.7 实现 `multimodal_cross_search()` 跨模态搜索
- [x] 10.8 实现 RRF (Reciprocal Rank Fusion) 融合
- [x] 10.9 实现 `multimodal_fusion_search()` 多模态融合搜索
- [x] 10.10 实现 `multimodal_hybrid_search()` 稀疏+密集混合搜索
- [x] 10.11 实现 `multimodal_save/load()` 持久化
- [x] 10.12 实现 `multimodal_destroy()` 销毁函数
- [x] 10.13 多模态索引核心实现完成（编译通过）

## 11. CMake 构建集成

- [x] 11.1 在 `src/index/inverted/CMakeLists.txt` 配置 GIN/GiST/FULLTEXT/Bitmap
- [x] 11.2 在 `src/index/block/CMakeLists.txt` 配置 BRIN
- [x] 11.3 在 `src/index/vector_index/CMakeLists.txt` 配置 IVF-PQ/LSH/OPQ/多模态
- [x] 11.4 新模块编译通过

## 12. 文档与总结

- [x] 12.1 更新 `include/index/` 下的索引总览头文件
- [x] 12.2 添加新索引的使用示例
- [x] 12.3 单元测试已完成（GIN/GiST/FULLTEXT/BRIN/Bitmap/IVF-PQ）
