/*
 * string.c —— 字符串操作实现
 *
 * ============================================================
 * KMP 算法核心思想
 * ============================================================
 * KMP（Knuth-Morris-Pratt）算法利用模式串自身的重复结构来避免匹配失败时
 * 主串指针的回溯。
 *
 * next 数组的定义：next[i] 表示 pattern[0..i-1] 的最长相等前后缀的长度。
 *
 * 示例：pattern = "ababc"
 *   i=0: next[0]=-1（哨兵）
 *   i=1: "a" 无前后缀，next[1]=0
 *   i=2: "ab"，前缀"a"≠后缀"b"，next[2]=0
 *   i=3: "aba"，前缀"a"==后缀"a"，next[3]=1
 *   i=4: "abab"，前缀"ab"==后缀"ab"，next[4]=2
 *
 * 匹配过程：
 *   text:   a b a b a b c
 *   pattern: a b a b c
 *   当匹配到 text[4]='a' ≠ pattern[4]='c' 时，j 跳到 next[4]=2
 *           即 pattern 右移 4-2=2 位继续比较，避免了从头开始。
 */

#include <ds/string.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 基础操作
 * ============================================================ */

void ds_string_reverse(char *str)
{
    size_t len;
    size_t i;
    char   tmp;

    if (!str) {
        return;
    }

    len = strlen(str);
    for (i = 0u; i < len / 2u; ++i) {
        tmp = str[i];
        str[i] = str[len - 1u - i];
        str[len - 1u - i] = tmp;
    }
}

void ds_string_to_lower(char *str)
{
    if (!str) {
        return;
    }
    for (size_t i = 0u; str[i] != '\0'; ++i) {
        str[i] = (char)tolower((unsigned char)str[i]);
    }
}

void ds_string_to_upper(char *str)
{
    if (!str) {
        return;
    }
    for (size_t i = 0u; str[i] != '\0'; ++i) {
        str[i] = (char)toupper((unsigned char)str[i]);
    }
}

bool ds_string_starts_with(const char *str, const char *prefix)
{
    size_t str_len;
    size_t prefix_len;

    if (!str || !prefix) {
        return false;
    }

    str_len = strlen(str);
    prefix_len = strlen(prefix);
    if (prefix_len > str_len) {
        return false;
    }

    return memcmp(str, prefix, prefix_len) == 0;
}

bool ds_string_ends_with(const char *str, const char *suffix)
{
    size_t str_len;
    size_t suffix_len;

    if (!str || !suffix) {
        return false;
    }

    str_len = strlen(str);
    suffix_len = strlen(suffix);
    if (suffix_len > str_len) {
        return false;
    }

    return memcmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

/* ============================================================
 * 修剪空白字符
 * ============================================================ */

/* 判断字符是否为空白（空格、制表符、换行符、回车） */
static int is_whitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

char *ds_string_trim(char *str)
{
    if (!str) {
        return NULL;
    }

    str = ds_string_trim_left(str);
    return ds_string_trim_right(str);
}

/*
 * ============================================================
 * 去除首部空白：找到第一个非空白字符，将后续内容前移
 *
 *   "   hello" → "hello"
 *    ^^^             ^
 *   跳过前3个空格    从 s[3] 开始复制到 s[0]
 * ============================================================ */
char *ds_string_trim_left(char *str)
{
    size_t i;
    size_t j;

    if (!str) {
        return NULL;
    }

    /* 找到第一个非空白字符的位置 */
    for (i = 0u; str[i] != '\0' && is_whitespace(str[i]); ++i) {
        /* empty */
    }

    if (i > 0u) {
        for (j = 0u; str[i] != '\0'; ++j, ++i) {
            str[j] = str[i];
        }
        str[j] = '\0';
    }

    return str;
}

/*
 * ============================================================
 * 去除尾部空白：从字符串末尾向前扫描，在第一个非空白字符后截断
 *
 *   "hello   " → "hello"
 *           ^^^
 *   从末尾向前，遇到空格就设 '\0'
 * ============================================================ */
char *ds_string_trim_right(char *str)
{
    size_t len;

    if (!str) {
        return NULL;
    }

    len = strlen(str);
    while (len > 0u && is_whitespace(str[len - 1u])) {
        --len;
        str[len] = '\0';
    }

    return str;
}

/* ============================================================
 * KMP 字符串搜索
 * ============================================================ */

/*
 * 构建 KMP next 数组。
 *
 * next[j] 的含义：当 pattern[j] 与主串字符不匹配时，
 * 下一次应该用 pattern[next[j]] 继续匹配。
 * 即 pattern[0..j-1] 的最长相等前后缀长度。
 */
static void kmp_build_next(const char *pattern, int *next, size_t len)
{
    int i; /* 后缀末尾位置 */
    int j; /* 前缀末尾位置（也是当前最长相等前后缀长度） */

    if (len == 0u) {
        return;
    }

    next[0] = -1; /* 哨兵：表示需要移动主串 */
    i = 0;
    j = -1;

    /*
     * 递推关系（循环不变量）：
     * 假设已知 next[i] = j（即 pattern[0..j-1] == pattern[i-j..i-1]）,
     * 现在计算 next[i+1]：
     *   若 pattern[i] == pattern[j]：next[i+1] = j + 1
     *   否则：j = next[j]（利用已算出的 next 值回退，类似匹配失败时的逻辑）
     */
    while (i < (int)len - 1) {
        if (j == -1 || pattern[i] == pattern[j]) {
            ++i;
            ++j;
            next[i] = j;
        } else {
            j = next[j];
        }
    }
}

/*
 * KMP 搜索主函数。
 *
 * 匹配过程：
 * - i 指向主串 text，j 指向模式串 pattern
 * - 当 text[i] == pattern[j] 时，i++、j++
 * - 当 j == len(pattern) 时，匹配成功，返回 i - j
 * - 当 text[i] ≠ pattern[j] 时，j = next[j]
 *   （利用 next 数组跳转到下一个可能匹配的位置，i 不回退！）
 */
int ds_string_kmp_search(const char *text, const char *pattern)
{
    size_t text_len;
    size_t pattern_len;
    int   *next;
    int    i; /* text 下标 */
    int    j; /* pattern 下标 */
    int    result;

    if (!text || !pattern) {
        return -1;
    }

    text_len = strlen(text);
    pattern_len = strlen(pattern);
    if (pattern_len == 0u) {
        return 0; /* 空模式串约定匹配在位置 0 */
    }
    if (pattern_len > text_len) {
        return -1;
    }

    next = (int *)malloc(sizeof(int) * pattern_len);
    if (!next) {
        return -1;
    }

    kmp_build_next(pattern, next, pattern_len);

    i = 0;
    j = 0;
    while (i < (int)text_len && j < (int)pattern_len) {
        if (j == -1 || text[i] == pattern[j]) {
            ++i;
            ++j;
        } else {
            j = next[j];
        }
    }

    result = (j == (int)pattern_len) ? i - j : -1;
    free(next);
    return result;
}

/* ============================================================
 * 字符串分割
 * ============================================================ */

/*
 * 按分隔符拆分字符串。
 *
 * 示例：str="a,b,c", delimiter=','
 *   第1次：找到 ',' 在 i=1，截取 "a"
 *   第2次：找到 ',' 在 i=3，截取 "b"
 *   第3次：到达结尾，截取 "c"
 *   返回：["a", "b", "c", NULL]
 *
 * 返回结构：连续内存块
 *   [char*][char*][char*][NULL][字符串数据...]
 *    ^-- parts 指针指向这里
 */
char **ds_string_split(const char *str, char delimiter, size_t max_count)
{
    size_t      len;
    size_t      part_count;
    size_t      alloc_size;
    char       *buffer;
    char      **parts;
    const char *start;
    size_t      part_index;

    if (!str) {
        return NULL;
    }

    len = strlen(str);
    if (len == 0u) {
        parts = (char **)calloc(1u, sizeof(char *));
        return parts; /* 返回只有 NULL 的数组 */
    }

    /* 第一遍：数分隔符确定段数 */
    part_count = 1u;
    for (size_t i = 0u; i < len; ++i) {
        if (str[i] == delimiter) {
            ++part_count;
        }
    }

    /*
     * 分配连续内存块：
     *   前 (part_count+1) 个槽位存 char* 指针（含末尾 NULL）
     *   后 (len+1) 字节存实际字符串数据
     */
    alloc_size = (part_count + 1u) * sizeof(char *) + len + 1u;
    buffer = (char *)calloc(1u, alloc_size);
    if (!buffer) {
        return NULL;
    }

    parts = (char **)buffer;
    /* 字符串数据紧跟在指针数组后面 */
    char *data_start = buffer + (part_count + 1u) * sizeof(char *);

    /*
     * 第二遍：遍历原字符串，将每段复制到 buffer 中
     * 用 start/end 标记每段的起止，用 memcpy 复制
     */
    start = str;
    part_index = 0u;

    for (const char *p = str; /* 在循环中更新 p */; ++p) {
        if (*p == delimiter || *p == '\0') {
            /* 检查是否达到最大分段数限制 */
            if (max_count > 0u && part_index >= max_count - 1u && *p != '\0') {
                /* 最后一段：包含剩余所有内容（含后续分隔符） */
                size_t remaining = strlen(start);
                memcpy(data_start, start, remaining + 1u);
                parts[part_index++] = data_start;
                data_start += remaining + 1u;
                break;
            }

            /* 正常分段：从 start 到 p 之间的内容 */
            size_t seg_len = (size_t)(p - start);
            memcpy(data_start, start, seg_len);
            data_start[seg_len] = '\0';
            parts[part_index++] = data_start;
            data_start += seg_len + 1u;

            if (*p == '\0') {
                break;
            }
            start = p + 1;
        }
    }

    parts[part_index] = NULL;
    return parts;
}

void ds_string_split_free(char **parts)
{
    free(parts);
}

size_t ds_string_split_count(char **parts)
{
    size_t count;

    if (!parts) {
        return 0u;
    }

    count = 0u;
    while (parts[count] != NULL) {
        ++count;
    }
    return count;
}
