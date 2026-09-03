#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ds/str.h"
#include "ds/data_type.h"
#include "ds/queue.h"

// todo: 待实现
int get_last_digit_of_string(const char *str)
{
    (void)str; // Suppress unused parameter warning
    return -1;
}

unsigned long long str_to_number(const char *str)
{
    if (str == NULL) {
        return 0;
    }

    int len = strlen(str);
    unsigned long long res = 0;
    for (int i = 0; i < len; i++) {
        if (IS_DIGIT(str[i])) {
            res = res * 10 + (str[i] - '0');
            // printf("ch is %c, res is %lld.\n", str[i], res);
        }
    }

    return res;
}

static long get_total_second(char *time_str, char *unit)
{
    char *end_ptr = NULL;
    long time = strtol(time_str, &end_ptr, 10);
    if (!end_ptr) {
        return 0;
    }

    if (strcmp(unit, "second") == 0) {
        return time;
    } else if (strcmp(unit, "minute") == 0) {
        return time * SECOND_PER_MINUTE;
    } else if (strcmp(unit, "hour") == 0) {
        return time * SECOND_PER_HOUR;
    } else if (strcmp(unit, "day") == 0) {
        return time * SECOND_PER_DAY;
    } else if (strcmp(unit, "month") == 0) {
        return time * SECOND_PER_MONTH;
    } else if (strcmp(unit, "year") == 0) {
        return time * SECONDE_PER_YEAR;
    }

    return 0;
}

static __inline__ void time_unit_single(long time, const char *unit, char **res)
{
    if (time == 0) {
        return;
    }

    char buf[50] = {0};
    sprintf(buf, "%ld %s ", time, unit);
    strcat(*res, buf);
}

char *time_unit(char *time_str)
{
    char *time = strtok(time_str, " ");
    char *unit = strtok(NULL, " ");
    long total_second = get_total_second(time, unit);

    char *res = (char *)malloc(sizeof(char) * 100);
    memset(res, 0, sizeof(char) * 100);
    // printf("res: %s, time: %s, unit: %s, total_second: %ld\n", res, time, unit, total_second);

    long year = total_second / SECONDE_PER_YEAR;
    time_unit_single(year, "year", &res);
    total_second -= year * SECONDE_PER_YEAR;
    // printf("year res: %s\n", res);

    long month = total_second / SECOND_PER_MONTH;
    time_unit_single(month, "month", &res);
    total_second -= month * SECOND_PER_MONTH;
    // printf("month res: %s\n", res);

    long day = total_second / SECOND_PER_DAY;
    time_unit_single(day, "day", &res);
    total_second -= day * SECOND_PER_DAY;
    // printf("day res: %s\n", res);

    long hour = total_second / SECOND_PER_HOUR;
    time_unit_single(hour, "hour", &res);
    total_second -= hour * SECOND_PER_HOUR;
    // printf("hour res: %s\n", res);

    long minute = total_second / SECOND_PER_MINUTE;
    time_unit_single(minute, "minute", &res);
    total_second -= minute * SECOND_PER_MINUTE;
    // printf("minute res: %s\n", res);

    time_unit_single(total_second, "second", &res);
    // printf("second res: %s\n", res);

    // 去掉末尾的空格（如果有）
    if (strlen(res) > 0 && res[strlen(res) - 1] == ' ') {
        res[strlen(res) - 1] = '\0';
    }

    return res;
}

static __inline__ void toLowerCase(char *str)
{
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

int unrepeat_word_count(word_count_map_t **map, char *string)
{
    char *token = strtok(string, " ");
    while (token != NULL) {
        toLowerCase(token);

        word_count_map_t *entry;
        HASH_FIND_STR(*map, token, entry);
        if (!entry) {
            entry = (word_count_map_t *)malloc(sizeof(word_count_map_t));
            strcpy(entry->key, token);
            entry->occurrence = 1;
            HASH_ADD_STR(*map, key, entry);
        } else {
            entry->occurrence++;
        }

        token = strtok(NULL, " ");
    }

    int word_cnt = 0;
    word_count_map_t *current, *tmp;
    HASH_ITER(hh, *map, current, tmp) {
        word_cnt++;;
        HASH_DEL(*map, current);
        free(current);
    }

    return word_cnt;
}

// -符号会出现在字符串的末尾表示换行, 可以和下一个字符串中的单词拼成1个单词, 统计二维数组中的单词个数
int get_word_cnt(char **lines, uint32_t lines_size)
{
    bool hash_word = false;
    bool pre_end_line = false;

    int word_cnt = 0;
    for (int i = 0; i < lines_size; i++) {
        char *ch = lines[i];

        // 前缀空格过滤掉
        while (pre_end_line && *ch == ' ') {
            ch++;
        }

        while (*ch) {
            if (islower(*ch) || isupper(*ch)) {
                hash_word = true;
            } else if (*ch == '-') {
                break;
            } else if (hash_word) {
                word_cnt++;
                hash_word = false;
            }
            ch++;
        }

        if (*ch) {
            pre_end_line = true;
        } else {
            pre_end_line = false;
            hash_word = false;
            word_cnt += 1;
        }
    }

    return word_cnt;
}

bool is_palindrome(const char *str)
{
    queue_t *queue = queue_create(10, QUEUE_TYPE_ARRAY);
    if (!queue) {
        return false;
    }

    for (int i = 0; i < strlen(str); i++) {
        if (enqueue(queue, TO_VOID_PTR(&str[i])) != STATUS_OK) {
            queue_destroy(queue);
            return false;
        }
    }

    int length = queue_length(queue);
    array_queue_t *arr_que = queue->data.array_queue;
    if (length > 1) {
        size_t front = arr_que->front;
        size_t rear = arr_que->rear;
        for (int i = 0; i < length / 2; i++) {
            char f = *(char *)arr_que->data[front];
            char s = *(char *)arr_que->data[(rear - 1 + arr_que->capacity) % arr_que->capacity];
            if (s != f) {
                queue_destroy(queue);
                return false;
            }

            front = (front + 1) % arr_que->capacity;
            rear = (rear - 1 + arr_que->capacity) % arr_que->capacity;
        }
    }

    queue_destroy(queue);
    return true;
}