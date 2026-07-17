#include "leetcode/leetcode.h"

#include <stdlib.h>
#include <string.h>

// leetcode 118
int **generate(int numRows, int *returnSize, int **returnColumnSizes)
{
    int **res = malloc(sizeof(int *) * numRows);
    *returnSize = numRows; 
    *returnColumnSizes = malloc(sizeof(int) * numRows);

    for (int i = 0; i < numRows; i++) {
        res[i] = malloc(sizeof(int) * (i + 1));
        (*returnColumnSizes)[i] = i + 1;
        res[i][0] = res[i][i] = 1;
        for (int j = 1; j < i; j++) {
            res[i][j] = res[i - 1][j] + res[i - 1][j - 1];
        }
    }

    return res;
}