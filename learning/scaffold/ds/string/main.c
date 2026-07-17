/**
 * @file main.c
 * @brief 数据结构字符串学习卡片 - 演示 C 字符串操作与编码处理
 *
 * 涵盖内容：
 * - C 字符串的本质（char 数组 + '\0' 结尾）
 * - 常用字符串函数：strlen, strcpy, strcat, strcmp
 * - 字符串的基本操作与 UTF-8 编码意识
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 工具函数 ==================== */

/**
 * 打印字符串及其长度
 * @param label 标签
 * @param s     C 字符串
 */
static void print_string(const char *label, const char *s)
{
    printf("  %s: \"%s\" (len=%zu)\n", label, s, strlen(s));
}

/**
 * 打印十六进制内存视图（用于观察 '\0' 结尾）
 * @param s 字符串
 * @param n 打印字节数
 */
static void print_hex(const char *label, const char *s, size_t n)
{
    printf("  %s hex: ", label);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 32 && c < 127) {
            printf("%c ", c);
        } else {
            printf("\\x%02X ", c);
        }
    }
    printf("\n");
}

/* ==================== 字符串基础演示 ==================== */

/**
 * 字符串基础演示
 * C 字符串是 char 类型的数组，以 '\0' (null terminator) 结尾
 */
static void demo_string_basics(void)
{
    printf("[string] 字符串基础演示\n");

    /* 方式1：字符数组，自动包含 '\0' */
    char arr[] = {'H', 'e', 'l', 'l', 'o', '\0'};
    printf("  char arr[] = {'H','e','l','l','o','\\0'}\n");
    printf("  数组大小: %zu, 字符串长度: %zu\n", sizeof(arr), strlen(arr));

    /* 方式2：字符串字面量，编译器自动加 '\0' */
    const char *lit = "Hello";
    printf("  const char *lit = \"Hello\"\n");
    printf("  字符串长度: %zu\n", strlen(lit));

    /* 观察 '\0' 结尾 */
    printf("  观察 '\\0' 结尾:\n");
    print_hex("arr", arr, 7);
    print_hex("lit", lit, 7);

    printf("\n");
}

/* ==================== 字符串函数演示 ==================== */

/**
 * 字符串函数演示
 * strlen, strcpy, strcat, strcmp 的使用与注意事项
 */
static void demo_string_functions(void)
{
    printf("[string] 字符串函数演示\n");

    char dest[64] = {0};

    /* strlen: 计算字符串长度（不含 '\0'） */
    const char *src = "Hello";
    printf("  strlen(\"%s\") = %zu\n", src, strlen(src));

    /* strcpy: 复制字符串（注意目标缓冲区大小） */
    strcpy(dest, src);
    printf("  strcpy(dest, \"%s\") => dest = \"%s\"\n", src, dest);

    /* strcat: 连接字符串 */
    strcat(dest, " World");
    printf("  strcat(dest, \" World\") => dest = \"%s\"\n", dest);

    /* strcmp: 比较字符串
     * 返回值: 0=相等, <0=前小, >0=前大
     * 按字典序逐字符比较 */
    int cmp1 = strcmp("apple", "banana");
    int cmp2 = strcmp("hello", "Hello");
    int cmp3 = strcmp("abc", "abc");
    printf("  strcmp(\"apple\", \"banana\") = %d\n", cmp1);
    printf("  strcmp(\"hello\", \"Hello\") = %d\n", cmp2);
    printf("  strcmp(\"abc\", \"abc\") = %d\n", cmp3);

    printf("\n");
}

/* ==================== 字符串操作演示 ==================== */

/**
 * 字符串基本操作演示
 */
static void demo_string_manipulation(void)
{
    printf("[string] 字符串操作演示\n");

    char buf[64] = "Hello World";

    /* 获取子串（手动复制） */
    char sub[16] = {0};
    size_t start = 6, len = 5;
    strncpy(sub, buf + start, len);
    sub[len] = '\0';  /* strncpy 不保证加 '\0'，需手动处理 */
    printf("  子串 [buf+%zu, %zu]: \"%s\"\n", start, len, sub);

    /* 大小写转换 */
    char upper[64] = "hello";
    for (char *p = upper; *p; p++) {
        if (*p >= 'a' && *p <= 'z') {
            *p = *p - ('a' - 'A');
        }
    }
    printf("  小写转大写: \"%s\"\n", upper);

    /* 反转字符串 */
    char rev[64] = "abcde";
    size_t l = 0, r = strlen(rev);
    while (l < r) {
        char tmp = rev[l];
        rev[l++] = rev[--r];
        rev[r] = tmp;
    }
    printf("  反转 \"abcde\": \"%s\"\n", rev);

    printf("\n");
}

/* ==================== UTF-8 编码意识 ==================== */

/**
 * UTF-8 编码意识演示
 * UTF-8 使用变长编码：
 *   1字节: 0xxxxxxx     (ASCII)
 *   2字节: 110xxxxx 10xxxxxx
 *   3字节: 1110xxxx 10xxxxxx 10xxxxxx  (中文在此范围)
 *   4字节: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 */
static void demo_utf8_awareness(void)
{
    printf("[string] UTF-8 编码意识演示\n");

    /* 中文字符串 */
    const char *chinese = "你好";
    printf("  中文字符串: \"%s\"\n", chinese);
    printf("  strlen(chinese) = %zu (字节数)\n", strlen(chinese));
    printf("  字符数 = 2 (一个中文 = 3 字节 UTF-8)\n");

    /* 打印十六进制 */
    printf("  hex: ");
    for (size_t i = 0; i < strlen(chinese); i++) {
        printf("%02X ", (unsigned char)chinese[i]);
    }
    printf("\n");

    /* 警告：strncpy 可能截断多字节字符 */
    char truncated[4] = {0};
    strncpy(truncated, chinese, 3);  /* 只复制了 3 字节，中文不完整 */
    printf("  strncpy(truncated, chinese, 3) => \"%s\" (乱码)\n", truncated);

    /* 正确做法：使用 mbstate_t 或第三方库处理 Unicode */
    printf("  注意：处理多字节字符需使用专门的 Unicode 库\n");

    printf("\n");
}

/* ==================== 动态字符串演示 ==================== */

/**
 * 动态字符串演示
 * 模拟一个简化的动态字符串结构
 */
typedef struct {
    char *data;   /* 数据存储 */
    size_t len;   /* 字符串长度（不含 '\0'） */
    size_t alloc; /* 已分配内存大小 */
} DynString;

/**
 * 创建动态字符串
 */
static DynString *dyn_str_create(const char *init)
{
    DynString *ds = (DynString *)malloc(sizeof(DynString));
    if (!ds) return NULL;
    size_t len = init ? strlen(init) : 0;
    ds->data = (char *)malloc(len + 1);
    if (!ds->data) { free(ds); return NULL; }
    if (init) {
        strcpy(ds->data, init);
    } else {
        ds->data[0] = '\0';
    }
    ds->len = len;
    ds->alloc = len + 1;
    return ds;
}

/**
 * 追加字符串（简化版，未做扩容优化）
 */
static void dyn_str_append(DynString *ds, const char *追加)
{
    size_t add_len = strlen(追加);
    size_t new_len = ds->len + add_len;
    /* 简化：每次都重新分配 */
    char *new_data = (char *)malloc(new_len + 1);
    strcpy(new_data, ds->data);
    strcat(new_data, 追加);
    free(ds->data);
    ds->data = new_data;
    ds->len = new_len;
    ds->alloc = new_len + 1;
}

/**
 * 释放动态字符串
 */
static void dyn_str_free(DynString *ds)
{
    if (ds) {
        free(ds->data);
        free(ds);
    }
}

static void demo_dynamic_string(void)
{
    printf("[string] 动态字符串演示\n");

    DynString *ds = dyn_str_create("Hello");
    printf("  创建: \"%s\" (len=%zu)\n", ds->data, ds->len);

    dyn_str_append(ds, " World");
    printf("  追加后: \"%s\" (len=%zu)\n", ds->data, ds->len);

    dyn_str_append(ds, "!");
    printf("  再追加后: \"%s\" (len=%zu)\n", ds->data, ds->len);

    dyn_str_free(ds);
    printf("\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 字符串 ===\n\n");

    demo_string_basics();
    demo_string_functions();
    demo_string_manipulation();
    demo_utf8_awareness();
    demo_dynamic_string();

    printf("=== PASS ===\n");
    return 0;
}
