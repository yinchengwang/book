# learning/include/

学习轨道公共头文件根。

`learning_includes` 接口库（定义于 `learning/CMakeLists.txt`）通过 `target_include_directories(learning_includes INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)` 暴露此目录给所有学习子库。

当前为空。学习子库的头文件按"目录镜像"约定：
- `learning/algo-c/` 内头文件直接位于该子目录下
- `learning/ds-c/orig/` 内头文件直接位于该子目录下
- 跨子库共享头文件（如公共类型定义）将迁入此目录
