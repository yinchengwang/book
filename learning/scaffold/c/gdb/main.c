/*
 * gdb scaffold — 故意埋三个 bug，gdb 找出来
 *
 * Bug A: count_vowels 字符集不含 'y' 且 size 写错
 * Bug B: print_vowel_table 数组下标越界（输入超长字符串）
 * Bug C: main 漏 +1，strlen(NULL) 空指针解引用
 *
 * 编译:
 *   gcc -g -O0 -Wall -Wextra -fsanitize=address -o buggy main.c
 *   # 或不加 sanitizer：gcc -g -O0 -o buggy main.c
 *
 * gdb 找 bug 三件套（详见 README）:
 *   (gdb) run           # 启动
 *   (gdb) bt            # backtrace 看 crash stack
 *   (gdb) frame 0       # 进具体栈帧
 *   (gdb) print x       # 看变量
 *   (gdb) list          # 看附近源码
 *
 * ASAN 会有更友好提示。本 demo 两个版本都跑通会验证 gdb 与 ASAN 都能定位。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOWELS_SIZE 10

static const char VOWELS[VOWELS_SIZE] = {'a', 'e', 'i', 'o', 'u', 'A', 'E', 'I', 'O', 'U'};

/* Bug A：size 写错（应为 VOWELS_SIZE），实测段错误或 wild read */
static int is_vowel(char c) {
    for (size_t i = 0; i < sizeof(VOWELS); i++) {  /* Bug A: sizeof(VOWELS) == sizeof(char)*size 在文件作用域里是 10，但写法误导 */
        if (VOWELS[i] == c) return 1;
    }
    return 0;
}

static int count_vowels(const char *s, size_t len) {
    if (!s) return 0;
    int n = 0;
    for (size_t i = 0; i < len; i++) {
        if (is_vowel(s[i])) n++;
    }
    return n;
}

/* Bug B：当 len > VOWELS_SIZE 时越界访问（buffer overflow） */
static void print_vowel_table(const char *s, size_t len) {
    int counts[VOWELS_SIZE] = {0};
    for (size_t i = 0; i < len; i++) {  /* Bug B: 不检查 s[i] 是否是 vowel，counts[idx] idx 可能越界 */
        int idx = (int)(s[i] - 'a');
        if (idx >= 0 && idx < VOWELS_SIZE) counts[idx]++;
    }
    fprintf(stderr, "[buggy] vowel table:\n");
    for (size_t i = 0; i < VOWELS_SIZE; i++) {
        if (counts[i]) fprintf(stderr, "  '%c' -> %d\n", VOWELS[i], counts[i]);
    }
}

int main(int argc, char **argv) {
    const char *input = NULL;  /* Bug C: 默认 NULL */
    if (argc >= 2) input = argv[1];

    /* Bug C：未检查 NULL 直接 strlen */
    size_t len = strlen(input);  /* 输入参数缺失时 segfault */

    int n = count_vowels(input, len);
    printf("[buggy] input length=%zu, vowel count=%d\n", len, n);
    print_vowel_table(input, len);
    return 0;
}
