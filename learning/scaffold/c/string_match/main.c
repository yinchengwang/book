/*
 * string_match scaffold — 字符串匹配算法四件套
 *
 * 演示四种经典字符串匹配算法的中间匹配过程：
 *   1. Brute Force — 朴素逐位比较
 *   2. KMP — lps 数组预计算 + 失配跳转
 *   3. Boyer-Moore 简化版 — 坏字符规则
 *   4. Rabin-Karp — 滚动哈希
 *
 * 复现：
 *   gcc -Wall -Wextra -O2 -std=c11 -o stringmatch_demo main.c
 *   ./stringmatch_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 1. Brute Force ==================== */

void brute_force(const char *text, const char *pattern) {
    int n = (int)strlen(text), m = (int)strlen(pattern);
    int found = 0;
    printf("[bf] text=\"%s\" pattern=\"%s\"\n", text, pattern);

    for (int i = 0; i <= n - m; i++) {
        int j;
        for (j = 0; j < m; j++) {
            printf("[bf step %d:%d] compare text[%d]='%c' vs pattern[%d]='%c'\n",
                   i, j, i + j, text[i + j], j, pattern[j]);
            if (text[i + j] != pattern[j]) {
                printf("[bf] mismatch at i=%d j=%d, shift→i=%d\n", i, j, i + 1);
                break;
            }
        }
        if (j == m) {
            printf("[bf] ✓ found at index %d\n", i);
            found++;
        }
    }
    if (!found) printf("[bf] ✗ not found\n");
}

/* ==================== 2. KMP ==================== */

void compute_lps(const char *pattern, int *lps, int m) {
    int len = 0;
    lps[0] = 0;
    int i = 1;

    printf("[kmp lps] building lps array for \"%s\":\n", pattern);
    printf("[kmp lps] lps[0] = 0\n");

    while (i < m) {
        if (pattern[i] == pattern[len]) {
            len++;
            lps[i] = len;
            printf("[kmp lps] lps[%d] = %d (pattern[%d]='%c' == pattern[%d]='%c')\n",
                   i, len, i, pattern[i], len - 1, pattern[len - 1]);
            i++;
        } else {
            if (len != 0) {
                printf("[kmp lps] fallback: len %d → lps[%d]=%d\n", len, len - 1, lps[len - 1]);
                len = lps[len - 1];
            } else {
                lps[i] = 0;
                printf("[kmp lps] lps[%d] = 0 (mismatch at len=0)\n", i);
                i++;
            }
        }
    }
    printf("[kmp lps] final lps table: [");
    for (int k = 0; k < m; k++)
        printf("%d%s", lps[k], k < m - 1 ? " " : "");
    printf("]\n");
}

void kmp(const char *text, const char *pattern) {
    int n = (int)strlen(text), m = (int)strlen(pattern);
    int *lps = (int *)malloc((size_t)m * sizeof(int));
    compute_lps(pattern, lps, m);

    printf("[kmp] text=\"%s\" pattern=\"%s\"\n", text, pattern);
    int i = 0, j = 0, found = 0;

    while (i < n) {
        if (pattern[j] == text[i]) {
            printf("[kmp step] i=%d j=%d match '%c'\n", i, j, text[i]);
            i++; j++;
        }
        if (j == m) {
            printf("[kmp] ✓ found at index %d\n", i - j);
            found++;
            j = lps[j - 1];
            printf("[kmp] after match: jump j→lps[%d]=%d\n", j, lps[j]);
        } else if (i < n && pattern[j] != text[i]) {
            if (j != 0) {
                printf("[kmp] mismatch at i=%d j=%d, jump j→lps[%d]=%d\n",
                       i, j, j - 1, lps[j - 1]);
                j = lps[j - 1];
            } else {
                printf("[kmp] mismatch at i=%d j=0, shift→i=%d\n", i, i + 1);
                i++;
            }
        }
    }
    if (!found) printf("[kmp] ✗ not found\n");
    free(lps);
}

/* ==================== 3. Boyer-Moore 简化版（仅坏字符） ==================== */

#define ALPHABET 256

void build_bad_char(const char *pattern, int m, int badchar[]) {
    for (int i = 0; i < ALPHABET; i++) badchar[i] = -1;
    for (int i = 0; i < m; i++) badchar[(unsigned char)pattern[i]] = i;
    printf("[bm bc] bad-char table (non -1 entries):\n");
    for (int i = 0; i < ALPHABET; i++)
        if (badchar[i] != -1)
            printf("[bm bc]   '%c' (ASCII %d) → last_seen=%d\n", i, i, badchar[i]);
}

void boyer_moore(const char *text, const char *pattern) {
    int n = (int)strlen(text), m = (int)strlen(pattern);
    int badchar[ALPHABET];
    build_bad_char(pattern, m, badchar);

    printf("[bm] text=\"%s\" pattern=\"%s\"\n", text, pattern);
    int s = 0, found = 0;

    while (s <= n - m) {
        int j = m - 1;
        while (j >= 0 && pattern[j] == text[s + j]) {
            printf("[bm step] s=%d j=%d match '%c'\n", s, j, text[s + j]);
            j--;
        }
        if (j < 0) {
            printf("[bm] ✓ found at index %d\n", s);
            found++;
            s += (s + m < n) ? m - badchar[(unsigned char)text[s + m]] : 1;
        } else {
            int shift = j - badchar[(unsigned char)text[s + j]];
            if (shift < 1) shift = 1;
            printf("[bm] mismatch at s=%d j=%d ('%c'!='%c'), shift=%d\n",
                   s, j, text[s + j], pattern[j], shift);
            s += shift;
        }
    }
    if (!found) printf("[bm] ✗ not found\n");
}

/* ==================== 4. Rabin-Karp ==================== */

#define RK_BASE 256
#define RK_MOD  101  /* 小质数做模数，便于演示 */

void rabin_karp(const char *text, const char *pattern) {
    int n = (int)strlen(text), m = (int)strlen(pattern);
    int phash = 0, thash = 0, h = 1, found = 0;

    /* h = BASE^(m-1) % MOD */
    for (int i = 0; i < m - 1; i++) h = (h * RK_BASE) % RK_MOD;

    /* 计算 pattern 和 text 首窗口的 hash */
    for (int i = 0; i < m; i++) {
        phash = (RK_BASE * phash + (unsigned char)pattern[i]) % RK_MOD;
        thash = (RK_BASE * thash + (unsigned char)text[i]) % RK_MOD;
    }
    printf("[rk] text=\"%s\" pattern=\"%s\"\n", text, pattern);
    printf("[rk] pattern hash = %d, window[0..%d] hash = %d\n", phash, m - 1, thash);

    for (int i = 0; i <= n - m; i++) {
        if (phash == thash) {
            /* hash 命中——逐字符确认 */
            int j;
            for (j = 0; j < m; j++)
                if (text[i + j] != pattern[j]) break;
            if (j == m) {
                printf("[rk] ✓ found at index %d (hash=%d)\n", i, thash);
                found++;
            } else {
                printf("[rk] hash collision at i=%d (hash=%d), false positive\n", i, thash);
            }
        } else {
            printf("[rk window %d] hash(text[i..i+%d])=%d != phash=%d, skip\n",
                   i, m - 1, thash, phash);
        }
        /* 滚动到下一窗口 */
        if (i < n - m) {
            thash = (RK_BASE * (thash - (unsigned char)text[i] * h) +
                     (unsigned char)text[i + m]) % RK_MOD;
            if (thash < 0) thash += RK_MOD;
        }
    }
    if (!found) printf("[rk] ✗ not found\n");
}

/* ==================== main ==================== */

int main(void) {
    const char *text = "ABABDABACDABABCABAB";
    printf("=== 字符串匹配算法演示 ===\n");
    printf("  测试文本: \"%s\"\n\n", text);

    brute_force(text, "ABABCABAB");
    printf("\n");

    kmp(text, "ABABCABAB");
    printf("\n");

    boyer_moore(text, "ABABCABAB");
    printf("\n");

    rabin_karp(text, "ABABCABAB");
    printf("\n");

    printf("=== PASS ===\n");
    return 0;
}
