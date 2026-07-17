# struct_union 学习笔记

## 概念地图

C struct/union 是 C 复合数据类型的骨架：

- **struct layout**：按声明顺序放字段；编译器自动 padding 让基础类型对齐到自身大小边界
- **大小端**：跨平台二进制格式必须 explicit endian；本仓库工程侧通常假设 little-endian（x86/ARM）
- **bit field**：`unsigned int f : 3;` 紧凑打包；网络协议（IP/TCP 头）必须用
- **union**：`sizeof(union) = max(sizeof, members)`；读时与写时类型必须一致，否则 type punning UB
- **flexible array member (FAM)**：`char data[]` 必须在 struct 末尾；只能 malloc 一次性分配
- **`_Alignas` / `_Alignof`**（C11）：显式指定对齐，与 SIMD/硬件 cache 行互动
- **`__attribute__((packed))`**：GCC 扩展；禁止 padding；用于网络包序列化

结构体的 ABI：跨编译器 ABI 不兼容（如 Linux System V ABI 与 Windows MSVC）；跨 long 宽度（Win64 LLP64 vs Unix LP64）也影响 struct layout——这是 NDK/JNI 痛点之一。

## 踩坑记录

1. **类型声明顺序**：本 demo 第一次写 `struct __attribute__((packed)) p packed_point { ... }`——`struct ... p ...` 不合法，会报"expected '=' or ',' or ';' before '{'"。正确是 `struct __attribute__((packed)) packed_point { ... }`。
2. **缺 stdlib.h**：用 `malloc` 必须 `#include <stdlib.h>`；本 demo 第一次漏掉被 `gcc -Wbuiltin-declaration-mismatch` 警告。
3. **unused variable**：演示 `struct point p = {...}` 看似没用——实际是为了让编译器接受字段值的初值。如果只想 `sizeof`，可用 `(struct point){0}` 复合字面量。Warnings 不影响功能。
4. **bit field 跨字节取址**：`unsigned int a: 1, b: 3;` 在不同编译器可能占 1 字节或 4 字节；ABI 不可靠。需要可移植位域时手动位运算 + 字节序列化（protobuf-nanopb 手法）。
5. **union 读写类型不一致是 UB**：写 int 读 float（C 标准允许但严格别名规则可能挑剔）；常见做法是 `memcpy` 复制后再读，避免 aliasing 歧义。
6. **FAM 必须末尾**：中途声明 FAM 编译错误；FAM 也不计入 sizeof——这是 malloc 时手工加的关键。
7. **GCC attribute 在 Clang 编译**：本 demo 用 GCC 扩展；Clang 接受但标准 C 不承认——跨编译器代码用 `_Alignas` 替代。

## 工程对照（≥100 字硬约束）

1. **`engineering/include/db/storage/bufpage.h`**：`PageHeader` 与 `HeapPageOpaque` 等结构体是 bufmgr 的页面元数据。含 padding 的 layout 与 LSN 等 uint64 字段。本卡刷过后能读懂 `offsetof(PageHeader, pd_lsn)` 断言。
2. **`engineering/include/db/consensus/raft_types.h`**：`RaftLogEntry` 是 Raft 日志项结构体；term、index、payload 字段顺序与大小端假设都是 ABI 协议。本卡读懂结构体如何映射 wire format。
3. **`engineering/src/algo-prod/quantizer.c`**：`PQCodebook` 包含 `centroids` flexible-style（C99 之前用 `centroids[1]` hack）；FAM 让码本按训练结果动态分配。
4. **`engineering/src/db/storage/wal_writer.c`**：`XLogRecord` 是 PG WAL 文件头结构体，沿用 PG 古老的 `xl_info : 8` bit field 紧凑布局——本卡读懂网络协议头序列化。
5. **`engineering/include/db/core/guc.h`**：GUC 配置变量结构体 `ConfigVariable` 含变长 name 与 value 字符串——本卡 FAM 可直接套用。

学完本卡能动手的事：

- 给 `engineering/include/db/` 加 `packing.h`，定义项目级 `ROCKSDB_PACKED` 宏 + `ASSERT_FIELD_OFFSET` 系列断言，将网络协议结构体（raft WAL entries / HTTP headers / RPC frames）固化。
- 在 `engineering/src/algo-prod/quantizer.c` 改 `centroids[1]` 为真 flexible array member，是 C99 标准化的清爽路径。
