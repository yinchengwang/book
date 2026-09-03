#include <stdio.h>
#include <stdlib.h>

#include "common.h"

// 17.14
int *smallestK(int *arr, int arrSize, int k, int *returnSize)
{
    if (arrSize == 0 || k == 0) {
        return NULL;
    }

    qsort(arr, arrSize, sizeof(int), int_cmp);

    int *res = (int *)malloc(sizeof(int) * k);
    for (int i = 0; i < k; i++) {
        res[i] = arr[i];
    }

    *returnSize = k;
    return res;
}