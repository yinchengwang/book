# file_io 学习笔记

## 概念地图

C 文件 I/O 分为两层：**标准 C 库（stdio）** 和 **POSIX 系统调用**：

- **stdio 层（FILE\*）**：
  - 缓冲 I/O，4KB 默认缓冲
  - 跨平台——Linux/macOS/Windows 都用同一套 API
  - API：`fopen/fclose/fread/fwrite/fprintf/fscanf/fgets/fputs/fseek/ftell/rewind`
- **POSIX 层（fd）**：
  - 无缓冲——每次 `read`/`write` 直接系统调用
  - 跨平台需 MinGW/Cygwin 提供；Windows 原生 API 是 `CreateFile`/`ReadFile`/`WriteFile`
  - API：`open/close/read/write/lseek/fsync`
- **两层关系**：`fopen` 内部调 `open`，`fread` 内部调 `read`，`fclose` 内部 `fflush + close`。**stdio 是 fd 之上的便利层**
- **缓冲三种模式**（`setvbuf`）：
  - `_IOFBF`（全缓冲）：写满缓冲区或 `fflush` 才输出。**文件默认**
  - `_IOLBF`（行缓冲）：遇 `\n` 自动 flush。**`stdout` 在连 TTY 时默认**
  - `_IONBF`（无缓冲）：每 write 立即输出。**`stderr` 默认**
- **文本模式 vs 二进制模式**：
  - `"w"`/`"r"`（文本模式）：Windows 上自动转换 `\n` ↔ `\r\n`
  - `"wb"`/`"rb"`（二进制模式）：不做转换，**二进制数据必须用此模式**
  - **Linux/macOS 上两者等价**——这是跨平台 C 代码的隐性 bug 来源

## 踩坑记录

1. **忘记 `fclose`/`close`**：进程退出时 OS 会回收 fd，但**缓冲数据可能丢失**——`fflush(fp); fclose(fp);` 是必须
2. **缓冲区刷新与日志可见性**：`fprintf(log_fp, "...")` 不立刻可见，因为缓冲。**调试时调 `setvbuf(log_fp, NULL, _IONBF, 0)` 或每次手动 `fflush`**
3. **`"w"` 模式截断文件**：`fopen(path, "w")` 不存在则创建，存在则清空。**想追加用 `"a"`**
4. **`fgets` 保留 `\n`**：`fgets(buf, n, fp)` 读到 `\n` 不丢弃——很多新手以为它会去掉
5. **二进制读写 struct 要 `fread(&s, sizeof(s), 1, fp)`**：一次读一个完整 struct，避免对齐 padding 跨多次读取出问题
6. **`ftell` 返回 `long` 不是 `size_t`**：2GB+ 文件用 `ftello`/`fseeko`（POSIX）或 `ftell64`/`fseek64`

## 工程对照（≥100 字硬约束）

文件 I/O 在本仓库 `engineering/` 中是基础设施：

1. **`engineering/src/db/storage/page/disk.c` 的页读写**：所有数据库页面通过 `fopen(..., "rb"/"wb")` 二进制模式读写——绝对不能文本模式（页内容含任意字节包括 `\r`/`\n`/空字节）。这是数据库引擎的最低层
2. **`engineering/src/db/core/structured_log.c` 的日志 append**：用 `fopen(path, "a")` 追加模式——日志只能增长，永不截断。每条 log 一次 `fprintf` + `fflush` 保证崩溃前可见
3. **`engineering/rag/src/rag/persist/hnsw_persist.c` 的 6 个 `fopen` 站点**：分别在 `hnsw_persist.c` 行 256、336、367、469、490——每个对应一个持久化阶段（写 meta/写图/读 meta/读图/读邻居）。**所有 fopen 都用 "wb"/"rb"**，绝无 "w"/"r"
4. **`engineering/src/db/subscription/cdc_wal.c` 的流式解析**：行 205 `parser->current_file = fopen(filename, "rb")` + 行 366 第二个 fopen——WAL 文件通常 GB 级，必须二进制 + 缓冲 + 循环读
5. **`engineering/src/db/storage/kv/kv.c` 和 `kv_ttl.c`**：KV 存储的 page-in/page-out 也走二进制 fread/fwrite——TTL 文件尤其需要"追加 + 定期 compaction"模式
6. **`engineering/src/db/storage/ts/ts_engine.c` 等时序存储**：每次 segment 写都用 `"wb"` 模式 + `fwrite` 完整 segment header——避免文本模式被转换

学完本卡能动手的事：扫描 `learning/scaffold/` 下所有 main.c，凡是创建临时文件的，统一改用 `tmpfile()` 或固定 `.tmp` 后缀 + `atexit(remove)` 注册清理，避免污染源码目录。