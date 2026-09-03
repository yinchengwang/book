// data_type.h - 数据类型定义
#ifndef ALGO_DS_DATA_TYPE_H
#define ALGO_DS_DATA_TYPE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    TYPE_INT,
    TYPE_STRING,
    TYPE_FLOAT
} data_type_t;

// 通用宏
#define TO_VOID_PTR(ptr) ((void *)(ptr))
#define VOID_PTR_TO(type, ptr) (*(type *)(ptr))

// 状态码
#define STATUS_OK 0
#define STATUS_ERR (-1)

// 时间单位常量
#define SECOND_PER_MINUTE 60
#define SECOND_PER_HOUR   (60 * 60)
#define SECOND_PER_DAY    (24 * 60 * 60)
#define SECOND_PER_MONTH  (30 * 24 * 60 * 60)   // 近似值
#define SECONDE_PER_YEAR  (365 * 24 * 60 * 60)  // 近似值

// 字符判断宏
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_ALPHA(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))
#define IS_ALNUM(c) (IS_DIGIT(c) || IS_ALPHA(c))

#endif // ALGO_DS_DATA_TYPE_H
