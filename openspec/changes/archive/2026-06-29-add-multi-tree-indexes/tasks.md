# 实现任务列表

## T-Tree 内存索引

- [x] 1. 创建 `src/index/tree/ttree/` 目录和 `CMakeLists.txt`
- [x] 2. 创建 `ttree_private.h` - 定义 T-Tree 节点和数据结构
- [x] 3. 创建 `include/index/tree_index/ttree.h` - 公共 API 头文件
- [x] 4. 创建 `ttree_core.c` - 创建/销毁/比较/节点操作
- [x] 5. 创建 `ttree_insert.c` - 插入操作（旋转/分裂）
- [x] 6. 创建 `ttree_delete.c` - 删除操作（借位/合并）
- [x] 7. 创建 `ttree_lookup.c` - 查找和范围查询
- [x] 8. 创建 `test/` 下的 T-Tree 测试文件
- [x] 9. 更新 `src/index/tree/CMakeLists.txt` 添加 ttree 子目录

## Skip List 索引

- [x] 10. 创建 `src/index/tree/skip_list/` 目录和 `CMakeLists.txt`
- [x] 11. 创建 `skip_list_private.h` - 定义 Skip List 节点
- [x] 12. 创建 `include/index/tree_index/skip_list.h` - 公共 API 头文件
- [x] 13. 创建 `skip_list_core.c` - 创建/销毁/节点创建
- [x] 14. 创建 `skip_list_insert.c` - 插入操作
- [x] 15. 创建 `skip_list_delete.c` - 删除操作
- [x] 16. 创建 `skip_list_lookup.c` - 查找和范围查询
- [x] 17. 创建 `test/` 下的 Skip List 测试文件
- [x] 18. 更新 `src/index/tree/CMakeLists.txt` 添加 skip_list 子目录

## Radix Tree 前缀索引

- [x] 19. 创建 `src/index/tree/radix_tree/` 目录和 `CMakeLists.txt`
- [x] 20. 创建 `radix_tree_private.h` - 定义 Radix Tree 节点
- [x] 21. 创建 `include/index/tree_index/radix_tree.h` - 公共 API 头文件
- [x] 22. 创建 `radix_tree_core.c` - 创建/销毁/节点操作
- [x] 23. 创建 `radix_tree_insert.c` - 插入操作（分裂/合并）
- [x] 24. 创建 `radix_tree_delete.c` - 删除操作
- [x] 25. 创建 `radix_tree_lookup.c` - 查找/前缀/最长前缀
- [x] 26. 创建 `test/` 下的 Radix Tree 测试文件
- [x] 27. 更新 `src/index/tree/CMakeLists.txt` 添加 radix_tree 子目录

## R-Tree 空间索引

- [x] 28. 创建 `src/index/tree/rtree/` 目录和 `CMakeLists.txt`
- [x] 29. 创建 `rtree_private.h` - 定义 R-Tree 节点和矩形
- [x] 30. 创建 `include/index/tree_index/rtree.h` - 公共 API 头文件
- [x] 31. 创建 `rtree_core.c` - 创建/销毁/节点操作
- [x] 32. 创建 `rtree_insert.c` - 插入操作（分裂算法）
- [x] 33. 创建 `rtree_delete.c` - 删除操作
- [x] 34. 创建 `rtree_search.c` - 矩形查询/迭代器
- [x] 35. 创建 `test/` 下的 R-Tree 测试文件
- [x] 36. 更新 `src/index/tree/CMakeLists.txt` 添加 rtree 子目录

## ART 自适应基数树

- [x] 37. 创建 `src/index/tree/art/` 目录和 `CMakeLists.txt`
- [x] 38. 创建 `art_private.h` - 定义 ART 节点类型（N4/N16/N48/N256）
- [x] 39. 创建 `include/index/tree_index/art.h` - 公共 API 头文件
- [x] 40. 创建 `art_core.c` - 创建/销毁/节点类型切换
- [x] 41. 创建 `art_insert.c` - 插入操作
- [x] 42. 创建 `art_delete.c` - 删除操作
- [x] 43. 创建 `art_lookup.c` - 查找操作
- [x] 44. 创建 `test/` 下的 ART 测试文件
- [x] 45. 更新 `src/index/tree/CMakeLists.txt` 添加 art 子目录

## 构建验证

- [x] 46. 运行 CMake 构建验证所有新索引编译通过
- [x] 47. 运行测试验证所有索引功能正常

## 文档

- [x] 48. 在 `notes/` 下添加 5 种索引的学习笔记