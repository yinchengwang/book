// 找出数组中任意一个重复的数字

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#define MAX_NUM 100

// 时间复杂度O(1), 额外空间复杂度O(N)
bool find_deplicate_number_hash(int *array, size_t arr_length, int *result)
{
    if (array == NULL || arr_length <= 0) {
        printf("[find_deplicate_number_hash] array is NULL.\n");
        return false;
    }

    // notice: 数组实现的hashmap需要知道原数据中的最大值以确定数组大小，否则hash_array[array[i]]会越界
    // TODO: 应该使用C语言的HASH_CREATE, HASH_ENTER, HASH_ADD系列函数
    int *hash_array = (int *)malloc(sizeof(int) * MAX_NUM);
    if (hash_array == NULL) {
        printf("[find_deplicate_number_hash] tmp array malloc failed.\n");
        return false;
    }
    memset(hash_array, 0, sizeof(int) * arr_length);

    for (int i = 0; i < arr_length; i++) {
        if (hash_array[array[i]] == 1) {
            *result = array[i];
            free(hash_array);
            return true;
        }
        hash_array[array[i]]++;
    }

    free(hash_array);
    printf("[find_deplicate_number_hash] There are no duplicate numbers in the array.\n");
    return false;
}

int cmp(const void *lhs, const void *rhs)
{
    return (*(int *)lhs - *(int *)rhs);
}

// 时间复杂度O(nlogn), 不允许修改原数组时额外空间复杂度O(N)
bool find_deplicate_number_sort(int *array, size_t arr_length, int *result)
{
    if (array == NULL || arr_length <= 0) {
        printf("[find_deplicate_number_sort] array is NULL.\n");
        return false;
    }

    int *tmp = (int *)malloc(sizeof(int) * arr_length);
    if (tmp == NULL) {
        printf("[find_deplicate_number_sort] tmp array malloc failed.\n");
        return false;
    }

    errno_t res = memcpy_s(tmp, sizeof(int) * arr_length, array, sizeof(int) * arr_length);
    if (res != 0) {
        free(tmp);
        printf("[find_deplicate_number_sort] tmp array memcpy_s failed.\n");
        return false;
    }

    qsort(tmp, arr_length, sizeof(int), cmp);

    for (int i = 0; i < arr_length; i++) {
        if (tmp[i] != i) {
            *result = tmp[i];
            free(tmp);
            return true;
        }
    }

    free(tmp);
    printf("[find_deplicate_number_sort] There are no duplicate numbers in the array.\n");
    return false;
}

// 时间复杂度O(1), 不允许修改原数组时额外空间复杂度O(N)
bool find_deplicate_number_algo(int *array, size_t arr_length, int *result)
{
    if (array == NULL || arr_length <= 0) {
        printf("[find_deplicate_number_algo] array is NULL.\n");
        return false;
    }

    int *tmp = (int *)malloc(sizeof(int) * arr_length);
    if (tmp == NULL) {
        printf("[find_deplicate_number_algo] tmp array malloc failed.\n");
        return false;
    }

    errno_t res = memcpy_s(tmp, sizeof(int) * arr_length, array, sizeof(int) * arr_length);
    if (res != 0) {
        free(tmp);
        printf("[find_deplicate_number_algo] tmp array memcpy_s failed.\n");
        return false;
    }

    // 如果位置i上的元素(x)不等于i, 则把x交换到位置x处, 如果位置x处已有元素已经和x相等，则当前x为重复的元素
    for (int i = 0; i < arr_length; i++) {
        while (array[i] != i) {
            if (array[i] == array[array[i]]) {
                *result = array[i];
                free(tmp);
                return true;
            }
            int tmp = array[i];
            array[i] = array[tmp];
            array[tmp] = tmp;
        }
    }

    free(tmp);
    printf("[find_deplicate_number_algo] There are no duplicate numbers in the array.\n");
    return false;
}

int main()
{
    int array[] = {1, 3, 2, 0, 3, 5, 2};

    int res = -1;
    bool is_deplicate = find_deplicate_number_hash(array, sizeof(array) / sizeof(int), &res);
    if (is_deplicate) {
        printf("deplicate number is: %d.\n", res);
    }

    is_deplicate = find_deplicate_number_sort(array, sizeof(array) / sizeof(int), &res);
    if (is_deplicate) {
        printf("deplicate number is: %d.\n", res);
    }

    is_deplicate = find_deplicate_number_algo(array, sizeof(array) / sizeof(int), &res);
    if (is_deplicate) {
        printf("deplicate number is: %d.\n", res);
    }

    // 异常输入
    is_deplicate = find_deplicate_number_hash(NULL, sizeof(array) / sizeof(int), &res);
    is_deplicate = find_deplicate_number_hash(array, 0, &res);
    is_deplicate = find_deplicate_number_sort(NULL, sizeof(array) / sizeof(int), &res);
    is_deplicate = find_deplicate_number_sort(array, 0, &res);
    is_deplicate = find_deplicate_number_algo(NULL, sizeof(array) / sizeof(int), &res);
    is_deplicate = find_deplicate_number_algo(array, 0, &res);

    // 无重复数据
    int arr[] = {1, 3, 2, 0, 4};
    is_deplicate = find_deplicate_number_hash(arr, sizeof(arr) / sizeof(int), &res);
    if (is_deplicate) {
        printf("deplicate number is: %d.\n", res);
    }
    is_deplicate = find_deplicate_number_sort(arr, sizeof(arr) / sizeof(int), &res);
    if (is_deplicate) {
        printf("deplicate number is: %d.\n", res);
    }
    is_deplicate = find_deplicate_number_algo(arr, sizeof(arr) / sizeof(int), &res);
    if (is_deplicate) {
        printf("deplicate number is: %d.\n", res);
    }

    return 0;
}