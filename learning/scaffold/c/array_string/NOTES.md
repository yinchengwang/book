# array_string 学习笔记

## 概念地图

C 语言的数组与字符串是同一片连续内存的两种视图：

- **数组的本质**：连续同类型元素的集合，无边界检查、无长度信息（`sizeof` 是编译期）。`int arr[5]` 在栈上分配 20 字节
- **行主序（Row-Major）布局**：C 的二维数组 `m[2][3]` 在内存中按行连续——`m[0][0] m[0][1] m[0][2] m[1][0] m[1][1] m[1][2]`。这与 Fortran 的列主序相反，跨语言数组交互时务必小心
- **VLA（C99）**：`int vla[n];` 长度由运行时变量决定，栈分配。C11 设为可选特性，**嵌入式/内核代码禁用 VLA**（栈大小不确定）
- **字符串字面量只读**：`char *p = "abc"` 中 `"abc"` 在 `.rodata` 段，`*p = 'x'` 会段错误。用 `const char *p` 更明确
- **`<string.h>` 三大族系**：
  - **长度/拷贝/拼接**：`strlen`/`strcpy`/`strncpy`/`strcat`/`strncat`——按 `\0` 终止符操作
  - **比较/查找**：`strcmp`/`strncmp`/`strchr`/`strrchr`/`strstr`——按字典序/字符比较
  - **内存级**：`memcpy`/`memmove`/`memset`/`memcmp`——按字节数操作，与 `\0` 无关
- **`memcpy` vs `memmove`**：源与目标重叠时 `memcpy` 是 UB（但 GCC 通常能正确处理），`memmove` 保证正确——按从后向前或从前向后复制

## 踩坑记录

1. **`strncpy` 不补 `\0`**：当源字符串长度 ≥ n 时，`strncpy` 不会写入 `\0`。**必须 `dst[n-1] = '\0'` 手动终止**
2. **`sizeof(arr)` 在函数参数中失效**：函数形参 `int arr[]` 实际退化为 `int *`，`sizeof arr` 在函数内是 8（指针）而非 20（数组）。**永远传入长度参数**
3. **字符串字面量 `*p = 'x'` 段错误**：MinGW 下可能"看起来正常"，但 Linux 下立刻崩——这是最隐蔽的跨平台 bug
4. **`memcpy(dst, src, n)` 重叠 UB**：用 `memmove` 代替，或交换 src/dst 顺序
5. **VLA 不能初始化**：`int vla[5] = {1,2,3,4,5}` 编译错误——VLA 必须运行时填值
6. **`strlen` O(n) 复杂度**：每次调用都遍历整个字符串。**缓存长度**比反复 `strlen` 高效

## 工程对照（≥100 字硬约束）

字符串与数组操作在本仓库 `engineering/` 中体现广泛：

1. **`snprintf` + `sizeof(key)` 模式**：`engineering/src/db/validator/sql_semantic.c` 行 37、269、303 三处 `snprintf(key, sizeof(key), "table:%s:meta", table_name)` 用 `sizeof(key)` 自动获取缓冲区大小，避免硬编码数字。这是 C99 之后字符串拼接的标准模式
2. **`vsnprintf` 错误消息**：`engineering/src/db/validator/semantic_analyzer.c` 行 31 `vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args)` 是 `printf` 风格错误消息的"安全版本"——`va_list` 处理变参，`sizeof` 自动限长
3. **`strncpy` 防溢出**：`engineering/rag/src/rag/persist/hnsw_persist.c` 行 46 `strncpy(result->error_msg, msg, sizeof(result->error_msg) - 1); result->error_msg[sizeof(result->error_msg) - 1] = '\0';` 是手动补 `\0` 的标准范式——比 `strcpy` 安全，但比 `snprintf` 啰嗦
4. **行主序的工程体现**：`engineering/src/db/storage/page/disk.c` 中所有页内数组都是行主序存储——`PageHeader + sizeof(PageHeader) + i*row_size` 计算每行起始地址
5. **VLA 缺席**：工程代码中**几乎不用 VLA**——`grep -rn "int .*\\[" engineering/src/db/storage/` 可验证，因为内核/数据库栈大小有限且要确定
6. **`memcpy` 在序列化层**：`engineering/rag/src/rag/persist/hnsw_persist.c` 序列化向量数据时直接 `memcpy(buf, vec->data, vec->dim * sizeof(float))`——零拷贝，按字节拷贝比循环赋值快 2-5 倍

学完本卡能动手的事：扫描 `learning/scaffold/` 下所有 main.c，把 `strcpy(dst, src)` 改为 `snprintf(dst, sizeof(dst), "%s", src)` 或 `strncpy(dst, src, sizeof(dst)-1); dst[sizeof(dst)-1]='\0';`。