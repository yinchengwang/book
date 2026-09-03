#include <stdio.h>
#include <stdlib.h>

int compare(const void *lhs, const void *rhs)
{
    return (*(int **)lhs)[0] - (*(int **)rhs)[0];
}

int query(int **items, int itemsSize, int q)
{
    int l = 0, r = itemsSize;
    while (l < r) {
        int mid = l + (r - l) / 2;
        if (items[mid][0] > q) {
            r = mid;
        } else {
            l = mid + 1;
        }
    }

    if (l == 0) {
        return 0;
    }

    return items[l - 1][1];
}

int* maximumBeauty(int** items, int itemsSize, int* itemsColSize, int* queries, int queriesSize, int* returnSize)
{
    qsort(items, itemsSize, sizeof(items[0]), compare);

    for (int i = 1; i < itemsSize; i++) {
        if (items[i][1] < items[i - 1][1]) {
            items[i][1] = items[i - 1][1];
        }
    }

    int *res = (int *)malloc(sizeof(int) * queriesSize);
    for (int i = 0; i < queriesSize; i++) {
        res[i] = query(items, itemsSize, queries[i]);
    }

    *returnSize = queriesSize;
    return res;
}

int main() {
    int itemsSize = 5;
    int itemsColSize = 2;
    int **items = (int **)malloc(itemsSize * sizeof(int *));
    for (int i = 0; i < itemsSize; i++) {
        items[i] = (int *)malloc(itemsColSize * sizeof(int));
    }

    items[0][0] = 1; items[0][1] = 2;
    items[1][0] = 3; items[1][1] = 2;
    items[2][0] = 2; items[2][1] = 4;
    items[3][0] = 5; items[3][1] = 6;
    items[4][0] = 3; items[4][1] = 5;

    // 定义queries数组
    int queries[6] = {1, 2, 3, 4, 5, 6};

    // 调用maximumBeauty函数
    int returnSize;
    int *result =
        maximumBeauty(items, itemsSize, &itemsColSize, queries, sizeof(queries) / sizeof(queries[0]), &returnSize);

    printf("Result: ");
    for (int i = 0; i < returnSize; i++) {
        printf("%d ", result[i]);
    }
    printf("\n");

    free(result);
    for (int i = 0; i < itemsSize; i++) {
        free(items[i]);
    }
    free(items);

    return 0;
}