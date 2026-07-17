/* array_string scaffold — 数组与字符串
 *
 * 复现命令：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *
 * 演示 5 段：
 *   [1d]     — 一维数组 + sizeof 求元素数
 *   [2d]     — 二维数组行主序布局
 *   [vla]    — C99 变长数组
 *   [str]    — 字符串字面量只读 + char[] vs char*
 *   [funcs]  — <string.h> 11 个函数演示
 */

#include <stdio.h>
#include <string.h>

int main(void) {
    /* === [1d] 一维数组 === */
    printf("[1d] === 一维数组 ===\n");
    int arr[5] = {10, 20, 30, 40, 50};
    printf("  arr           = %zu bytes\n", sizeof(arr));
    printf("  arr[0]..arr[4]= %d %d %d %d %d\n",
           arr[0], arr[1], arr[2], arr[3], arr[4]);
    printf("  元素数         = %zu\n", sizeof(arr) / sizeof(arr[0]));

    /* === [2d] 二维数组行主序 === */
    printf("\n[2d] === 二维数组行主序布局 ===\n");
    int m[2][3] = {{1,2,3},{4,5,6}};
    printf("  m[0][0..2]    = %d %d %d\n", m[0][0], m[0][1], m[0][2]);
    printf("  m[1][0..2]    = %d %d %d\n", m[1][0], m[1][1], m[1][2]);
    printf("  m 总字节       = %zu = 6 * %zu\n",
           sizeof(m), sizeof(m[0][0]));
    /* 行主序：m[i][j] 与 m[0][i*3+j] 地址相同 */
    int *p = &m[0][0];
    printf("  线性遍历       = %d %d %d %d %d %d\n",
           p[0], p[1], p[2], p[3], p[4], p[5]);

    /* === [vla] C99 变长数组 === */
    printf("\n[vla] === C99 变长数组 ===\n");
    int n = 4;
    int vla[n];                   /* 长度由运行时变量决定 */
    for (int i = 0; i < n; i++) vla[i] = i * 10;
    printf("  vla[%d]        = ", n);
    for (int i = 0; i < n; i++) printf("%d ", vla[i]);
    printf("\n  vla 字节       = %zu (栈分配)\n", sizeof(vla));

    /* === [str] 字符串字面量只读 === */
    printf("\n[str] === 字符串字面量只读 ===\n");
    char buf[] = "hello";          /* 拷贝到栈，可写 */
    char *p_lit = "world";         /* 指向只读段，*p_lit = 'x' 会段错误 */
    printf("  buf           = \"%s\" (栈，可写)\n", buf);
    printf("  p_lit         = \"%s\" (只读段)\n", p_lit);
    buf[0] = 'H';                  /* OK */
    printf("  buf[0]='H' 后 = \"%s\"\n", buf);
    /* p_lit[0] = 'W';  <-- 段错误，未执行 */

    /* === [funcs] <string.h> 11 函数演示 === */
    printf("\n[funcs] === <string.h> 11 函数 ===\n");

    const char *s = "Hello, World!";

    /* strlen — 长度（不含 \0） */
    printf("  strlen(\"%s\") = %zu\n", s, strlen(s));

    /* strcpy / strncpy — 拷贝 */
    char dst[20];
    strcpy(dst, s);
    printf("  strcpy dst    = \"%s\"\n", dst);
    strncpy(dst, "ABCDE", 3);     /* 不保证 \0 结尾 */
    dst[3] = '\0';
    printf("  strncpy dst   = \"%s\" (3 字节覆盖)\n", dst);

    /* strcat / strncat — 拼接 */
    char buf2[20] = "Hello";
    strcat(buf2, ", World");
    printf("  strcat buf2   = \"%s\"\n", buf2);

    /* strcmp / strncmp — 比较 */
    printf("  strcmp         = %d (\"abc\" vs \"abd\")\n", strcmp("abc", "abd"));
    printf("  strncmp        = %d (\"abc\" vs \"abd\", n=2)\n", strncmp("abc", "abd", 2));

    /* strchr / strrchr — 字符查找 */
    const char *found = strchr(s, 'W');
    printf("  strchr 'W'     = \"%s\" (offset %ld)\n", found ? found : "(null)",
           found ? (long)(found - s) : -1L);

    /* strstr — 子串查找 */
    const char *sub = strstr(s, "World");
    printf("  strstr \"World\" = \"%s\"\n", sub ? sub : "(null)");

    /* memcpy — 内存拷贝（不检查 \0） */
    int src[3] = {1, 2, 3};
    int dst2[3];
    memcpy(dst2, src, sizeof(src));
    printf("  memcpy dst2   = %d %d %d\n", dst2[0], dst2[1], dst2[2]);

    /* memmove — 重叠安全 */
    char overlap[10] = "123456789";
    memmove(overlap + 2, overlap, 5);  /* 移动 "12345" 到 [2..6] */
    printf("  memmove       = \"%s\" (重叠安全)\n", overlap);

    /* memset — 填充 */
    char zeros[10];
    memset(zeros, 0, sizeof(zeros));
    printf("  memset zeros  = 全 0 (前 4 字节: %02x %02x %02x %02x)\n",
           (unsigned char)zeros[0], (unsigned char)zeros[1],
           (unsigned char)zeros[2], (unsigned char)zeros[3]);

    printf("\n=== PASS ===\n");
    return 0;
}