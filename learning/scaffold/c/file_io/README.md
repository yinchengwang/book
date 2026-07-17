# file_io scaffold

C 文件 I/O 与标准 IO 演示——`fopen/fread/fwrite/fseek` vs `open/read/write/lseek` + `setvbuf` 三种缓冲模式。

## 复现命令

```bash
cd learning/scaffold/file_io

gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
```

## 预期输出（节选）

```
[fopen] === fopen 文本写 ===
  写入 3 行到 fileio_demo.tmp

[fopen] === fopen 文本读 ===
  line 1: line 1: hello
  line 2: line 2: world
  line 3: line 3: fputs without \n

[fread] === fread/fwrite 二进制读写 ===
  fwrite 3 ints
  fread 3 ints: 0x12345678 0xdeadbeef 0xcafebabe

[fseek] === fseek/ftell 随机访问 ===
  文件大小 = 50 bytes
  前 6 字节 = "line 1"
  末 6 字节 = "thout "

[open] === open/read/write/close POSIX 系统调用 ===
  write 21 bytes via fd=3
  read  21 bytes via fd=3: "POSIX syscall write"
  lseek to offset 7
  read from offset 7: "syscall write"

[buffer] === setvbuf 三种缓冲模式 ===
  _IOFBF 4KB: fflush 后强制写入
  _IOLBF 256: \n 后自动 flush
  _IONBF 0: 每 write 都立即输出

=== PASS ===
```

## 关键点

- **FILE\* 是缓冲 I/O，fd 是裸系统调用**：`fopen` 在 `open` 之上加了 4KB 缓冲区；`fwrite` 写到缓冲区，满后再 `write(fd,...)`
- **`"w"` vs `"wb"`**：Windows 上 `"w"` 把 `\n` 翻译成 `\r\n`，`"wb"` 保持原样。**Linux/macOS 上两者等价**
- **`fseek(fd, 0, SEEK_END)` + `ftell` 拿文件大小**：比 `stat()` 快（无需额外系统调用）
- **`setvbuf` 三模式**：`_IOFBF`（全缓冲，文件默认）、`_IOLBF`（行缓冲，`stdout` 默认连 TTY 时）、`_IONBF`（无缓冲，`stderr` 默认）

详见 NOTES.md 工程对照段。