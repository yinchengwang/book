#include <stdio.h>
#include <stdint.h>
#include <string.h>

// 近些年来，我国防沙治沙取得显著成果。某沙漠新种植N棵胡杨（编号1-N），排成一排。
// 一个月后，有M棵胡杨未能成活。
// 现可补种胡杨K棵，请问如何补种（只能补种，不能新种），可以得到最多的连续胡杨树？

// 输入
// N 总种植数量，1 <= N <= 100000
// M 未成活胡杨数量，M 个空格分隔的数，按编号从小到大排列，1 <= M <= N
// K 最多可以补种的数量，0 <= K <= M

// 输出
// 最多的连续胡杨棵树

// 5
// 2
// 2 4
// 1

// 3

// 10
// 3
// 2 4 7
// 1

// 6
int max_continuous_trees(int N, int *dead_positions, int M, int K)
{
    int left = 0;
    int max_length = 0;
    for (uint32_t right = 0; right < M; right++) {
        while ((right - left + 1) > K) {
            left++;
        }

        int start = (left > 0) ? dead_positions[left - 1] : 0;
        int end = (right + 1 < M) ? dead_positions[right + 1] : N + 1;
        int current_length = end - start - 1;
        if (current_length > max_length) {
            max_length = current_length;
        }
    }

    return max_length;
}

// 给你一个字符串 s，字符串s首尾相连成一个环形，
// 请你在环中找出 ‘o’ 字符出现了偶数次最长子字符串的长度。

// 输入：输入是一串小写字母组成的字符串
// 输出：输出是一个整数
int32_t longest_even_o_substring(char *s)
{
    int32_t n = strlen(s);
    int32_t total_o = 0;

    for (int32_t i = 0; i < n; i++) {
        if (s[i] == 'o') {
            total_o++;
        }
    }
    
    // 如果 'o' 的总数是偶数，整个字符串就是最长的
    if (total_o % 2 == 0) {
        return n;
    } else {
        return n - 1;
    }
}