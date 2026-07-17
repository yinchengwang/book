/*
 * str.h - 字符串处理函数
 *
 * 提供常用的字符串处理功能
 */
#ifndef ALGO_DS_STR_H
#define ALGO_DS_STR_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "ds/data_type.h"
#include "uthash/uthash.h"

#ifdef __cplusplus
extern "C" {
#endif

// 单词计数映射结构（用于 unrepeat_word_count）
typedef struct {
    char key[50];
    int occurrence;
    UT_hash_handle hh;
} word_count_map_t;

/* ========== 数字处理 ========== */

/**
 * @brief 获取字符串中最后一个数字
 * @param str 输入字符串
 * @return 最后一个数字字符，失败返回 -1
 */
int get_last_digit_of_string(const char *str);

/**
 * @brief 将字符串中的数字字符转换为整数
 * @param str 输入字符串
 * @return 转换后的整数
 */
unsigned long long str_to_number(const char *str);

/* ========== 时间转换 ========== */

/**
 * @brief 将时间字符串转换为人类可读格式
 * @param time_str 格式: "number unit"，如 "3601 second"
 * @return 转换后的字符串（调用者需 free），失败返回 NULL
 *
 * 示例:
 *   "3601 second" -> "1 hour 1 second"
 *   "1520 minute" -> "1 day 1 hour 20 minute"
 */
char *time_unit(char *time_str);

/* ========== 单词处理 ========== */

/**
 * @brief 统计字符串中不重复单词的数量
 * @param map 输出参数，指向 word_count_map_t 指针（函数内部会释放）
 * @param string 输入字符串（会被 strtok 修改）
 * @return 不重复单词的数量
 */
int unrepeat_word_count(word_count_map_t **map, char *string);

/**
 * @brief 统计多行字符串中的单词数量
 * @param lines 行数组
 * @param lines_size 行数
 * @return 单词数量
 *
 * 规则:
 *   - '-' 在行末表示与下一行拼接
 *   - 忽略前导空格（当与 '-' 拼接时）
 */
int get_word_cnt(char **lines, uint32_t lines_size);

/* ========== 回文判断 ========== */

/**
 * @brief 判断字符串是否是回文
 * @param str 输入字符串
 * @return 是否为回文
 */
bool is_palindrome(const char *str);

/* ========== 演示 ========== */

void ds_string_demo(void);

#ifdef __cplusplus
}
#endif

#endif // ALGO_DS_STR_H
