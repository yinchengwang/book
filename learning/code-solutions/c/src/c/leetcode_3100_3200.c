#include "common.h"
#include <stdlib.h>

// 3169
int countDays(int days, int** meetings, int meetingsSize, int* meetingsColSize)
{
    (void)meetingsColSize; // Suppress unused parameter warning

    qsort(meetings, meetingsSize, sizeof(int *), int_array_cmp);

    int l = 1, r = 0;
    for (int i = 0; i < meetingsSize; i++) {
        int start = meetings[i][0];
        int end = meetings[i][1];

        if (start > r) {
            days -= (r - l + 1);
            l = start;
        }

        if (end > r) {
            r = end;
        }
    }

    days -= (r - l + 1);
    return days;
}