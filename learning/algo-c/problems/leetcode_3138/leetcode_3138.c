#include <stdio.h>
#include <string.h>
#include<stdbool.h>

bool check(char *s, int cp, int len)
{
    int count_0[26] = {0};
    for (int j = 0; j < len; j += cp) {
        int count_1[26] = {0};
        for(int k = j; k < j + cp; k++) {
            count_1[s[k] - 'a']++;
        }

        if (j > 0 && memcmp(count_0, count_1, sizeof(int) * 26) != 0) {
            return false;
        }

        memcpy(count_0, count_1, sizeof(int) * 26);
    }

    return true;
}

int minAnagramLength(char* s)
{
    int len = strlen(s);
    for (int i = 1; i < len; i++) {
        if (len % i != 0) {
            continue;
        }

        if (check(s, i, len)) {
            return i;
        }
    }

    return len;
}