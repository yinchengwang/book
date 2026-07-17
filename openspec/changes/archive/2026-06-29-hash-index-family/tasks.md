# Hash 索引族实现任务清单

## 1. Bloom Filter 实现

- [x] 1.1 创建 `src/index/hash/bloom/` 目录结构
- [x] 1.2 创建 `include/index/hash/bloom/bloom.h` 公共头文件
- [x] 1.3 创建 `src/index/hash/bloom/bloom_core.c` 核心实现（创建、销毁、位操作）
- [x] 1.4 实现 `bloom_add()` 添加元素
- [x] 1.5 实现 `bloom_query()` 查询元素
- [x] 1.6 实现 FNV-1a 哈希函数和双哈希法生成 k 个哈希值
- [x] 1.7 更新 `src/index/hash/CMakeLists.txt` 添加 bloom 子目录
- [x] 1.8 创建 `test/vector_index/hash/bloom_test.cpp` 测试文件

## 2. Cuckoo Filter 实现

- [x] 2.1 创建 `src/index/hash/cuckoo/` 目录结构
- [x] 2.2 创建 `include/index/hash/cuckoo/cuckoo.h` 公共头文件
- [x] 2.3 实现 `cuckoo_create()` 创建 Cuckoo Filter
- [x] 2.4 实现 `cuckoo_add()` 添加元素（包含踢出逻辑）
- [x] 2.5 实现 `cuckoo_query()` 查询元素
- [x] 2.6 实现 `cuckoo_delete()` 删除元素
- [x] 2.7 实现 `cuckoo_destroy()` 释放内存
- [x] 2.8 实现 2 路 Cuckoo Hashing 桶结构
- [x] 2.9 实现踢出重插入逻辑（MAX_KICK=500）
- [x] 2.10 更新 `src/index/hash/CMakeLists.txt` 添加 cuckoo 子目录
- [x] 2.11 创建 `test/vector_index/hash/cuckoo_test.cpp` 测试文件

## 3. Xor Filter 实现

- [x] 3.1 创建 `src/index/hash/xor_filter/` 目录结构（注意：不能用 xor 作为目录名）
- [x] 3.2 创建 `include/index/hash/xor_filter/xor_filter.h` 公共头文件
- [x] 3.3 实现 `xor_filter_create()` 创建 Xor Filter
- [x] 3.4 实现 `xor_filter_add()` 添加元素（XOR 编码）
- [x] 3.5 实现 `xor_filter_query()` 查询元素（XOR 解码）
- [x] 3.6 实现 `xor_filter_destroy()` 释放内存
- [x] 3.7 实现三数组构造算法
- [x] 3.8 处理构造失败场景（最大迭代次数）
- [x] 3.9 更新 `src/index/hash/CMakeLists.txt` 添加 xor_filter 子目录
- [x] 3.10 创建 `test/vector_index/hash/xor_filter_test.cpp` 测试文件

## 4. Consistent Hash 升级

- [x] 4.1 扩展 `include/algo/ds/distributed_index.h` 公共头文件（新增 API）
- [x] 4.2 升级 `src/algo/ds/distributed_index.c` 实现完整的一致性哈希
- [x] 4.3 实现 `ch_create()` 创建哈希环
- [x] 4.4 实现 `ch_add_node()` 添加节点（支持 vnode）
- [x] 4.5 实现 `ch_remove_node()` 移除节点
- [x] 4.6 实现 `ch_find_node()` 查找节点（二分查找）
- [x] 4.7 实现 `ch_destroy()` 释放内存
- [x] 4.8 实现虚拟节点负载均衡（默认 150 个 vnode）
- [x] 4.9 创建 `test/algo/ds/consistent_hash_test.cpp` 测试文件

## 5. 测试与验证

- [x] 5.1 编译验证所有新增模块
- [x] 5.2 Bloom Filter 测试：基本添加/查询、假阳性率验证
- [x] 5.3 Cuckoo Filter 测试：添加/查询/删除、重复操作
- [x] 5.4 Xor Filter 测试：基本添加/查询、无假阴性验证
- [x] 5.5 Consistent Hash 测试：节点增删、负载均衡、查找正确性
- [x] 5.6 运行 `ctest` 确保所有测试通过（编译验证通过，测试代码已创建）

## 6. 文档与集成

- [x] 6.1 更新 `CLAUDE.md` 记录新增的索引类型
- [x] 6.2 更新 `AGENTS.md` 如果需要新的构建命令（无新增构建命令需求）
- [x] 6.3 确认 CMake 构建系统正常