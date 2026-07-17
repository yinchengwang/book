/* file_io scaffold — 文件 I/O 与标准 IO
 *
 * 复现命令：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *
 * 演示 5 段：
 *   [fopen]    — fopen/fclose 文本与二进制模式
 *   [fread]    — fread/fwrite 缓冲读写
 *   [fseek]    — fseek/ftell 随机访问
 *   [open]     — open/read/write/close POSIX 系统调用（MinGW 提供）
 *   [buffer]   — setvbuf 三种缓冲模式
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>      /* open/O_RDONLY/O_WRONLY/O_CREAT/O_TRUNC */
#include <unistd.h>     /* read/write/close/lseek */
#include <sys/stat.h>   /* mode_t / S_IRUSR|S_IWUSR */

#define TEST_PATH   "fileio_demo.tmp"

static void write_lines_via_stdio(void) {
    /* === [fopen] fopen 文本写 === */
    printf("[fopen] === fopen 文本写 ===\n");

    FILE *fp = fopen(TEST_PATH, "w");
    if (!fp) { perror("fopen w"); return; }

    fprintf(fp, "line 1: hello\n");
    fprintf(fp, "line 2: world\n");
    fputs("line 3: fputs without \\n", fp);
    fputc('\n', fp);

    fclose(fp);
    printf("  写入 3 行到 %s\n", TEST_PATH);
}

static void read_lines_via_stdio(void) {
    /* === [fopen] fopen 文本读 + 二进制读 === */
    printf("\n[fopen] === fopen 文本读 ===\n");

    FILE *fp = fopen(TEST_PATH, "r");
    if (!fp) { perror("fopen r"); return; }

    char line[128];
    int n = 0;
    while (fgets(line, sizeof(line), fp)) {
        printf("  line %d: %s", ++n, line);
    }
    fclose(fp);

    /* 二进制模式：fopen(..., "rb") / "wb" 不转换 \n */
    printf("\n[fread] === fread/fwrite 二进制读写 ===\n");
    FILE *wfp = fopen(TEST_PATH ".bin", "wb");
    if (!wfp) { perror("fopen wb"); return; }

    int data[] = {0x12345678, 0xdeadbeef, 0xcafebabe};
    size_t written = fwrite(data, sizeof(int), 3, wfp);
    printf("  fwrite %zu ints\n", written);
    fclose(wfp);

    FILE *rfp = fopen(TEST_PATH ".bin", "rb");
    if (!rfp) { perror("fopen rb"); return; }

    int read_back[3];
    size_t got = fread(read_back, sizeof(int), 3, rfp);
    printf("  fread %zu ints: 0x%08x 0x%08x 0x%08x\n",
           got, read_back[0], read_back[1], read_back[2]);
    fclose(rfp);
}

static void random_access(void) {
    /* === [fseek] 随机访问 === */
    printf("\n[fseek] === fseek/ftell 随机访问 ===\n");

    FILE *fp = fopen(TEST_PATH, "r");
    if (!fp) { perror("fopen r"); return; }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    printf("  文件大小 = %ld bytes\n", size);

    fseek(fp, 0, SEEK_SET);          /* 回到开头 */
    char buf[16] = {0};
    fread(buf, 1, 6, fp);            /* 读前 6 字节 */
    printf("  前 6 字节 = \"%s\"\n", buf);

    fseek(fp, -6, SEEK_END);         /* 最后 6 字节 */
    fread(buf, 1, 6, fp);
    buf[6] = '\0';
    printf("  末 6 字节 = \"%s\"\n", buf);

    fclose(fp);
}

static void posix_syscalls(void) {
    /* === [open] POSIX 系统调用 === */
    printf("\n[open] === open/read/write/close POSIX 系统调用 ===\n");

    /* open 返回文件描述符 fd（int），不是 FILE* */
    int fd = open(TEST_PATH ".posix", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) { perror("open"); return; }

    const char *msg = "POSIX syscall write\n";
    ssize_t n = write(fd, msg, strlen(msg));
    printf("  write %zd bytes via fd=%d\n", n, fd);
    close(fd);

    /* 读回 */
    fd = open(TEST_PATH ".posix", O_RDONLY);
    if (fd < 0) { perror("open r"); return; }

    char buf[64] = {0};
    n = read(fd, buf, sizeof(buf) - 1);
    printf("  read  %zd bytes via fd=%d: \"%s\"\n", n, fd, buf);
    close(fd);

    /* lseek 随机访问 */
    fd = open(TEST_PATH ".posix", O_RDONLY);
    if (fd >= 0) {
        off_t off = lseek(fd, 7, SEEK_SET);  /* 跳过 "POSIX s" */
        printf("  lseek to offset %lld\n", (long long)off);
        n = read(fd, buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("  read from offset 7: \"%s\"\n", buf);
        close(fd);
    }
}

static void buffering_demo(void) {
    /* === [buffer] setvbuf 三种缓冲模式 === */
    printf("\n[buffer] === setvbuf 三种缓冲模式 ===\n");

    /* 行缓冲（_IOLBF）：遇 \n 刷新，stdout 默认 */
    /* 全缓冲（_IOFBF）：缓冲区满才刷新，文件默认 */
    /* 无缓冲（_IONBF）：每 write 立即输出，stderr 默认 */

    FILE *fp = fopen(TEST_PATH ".buf", "w");
    if (!fp) return;

    setvbuf(fp, NULL, _IOFBF, 4096);  /* 全缓冲，4KB */
    fprintf(fp, "full buffered line 1\n");
    fprintf(fp, "full buffered line 2\n");
    fflush(fp);                       /* 手动刷新 */
    printf("  _IOFBF 4KB: fflush 后强制写入\n");

    setvbuf(fp, NULL, _IOLBF, 256);   /* 行缓冲 */
    fprintf(fp, "line buffered\n");
    fflush(fp);
    printf("  _IOLBF 256: \\n 后自动 flush\n");

    setvbuf(fp, NULL, _IONBF, 0);     /* 无缓冲 */
    fprintf(fp, "unbuffered\n");
    printf("  _IONBF 0: 每 write 都立即输出\n");

    fclose(fp);
}

int main(void) {
    write_lines_via_stdio();
    read_lines_via_stdio();
    random_access();
    posix_syscalls();
    buffering_demo();

    /* 清理临时文件 */
    remove(TEST_PATH);
    remove(TEST_PATH ".bin");
    remove(TEST_PATH ".posix");
    remove(TEST_PATH ".buf");

    printf("\n=== PASS ===\n");
    return 0;
}