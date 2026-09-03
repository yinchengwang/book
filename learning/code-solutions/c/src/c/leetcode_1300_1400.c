#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "leetcode/leetcode.h"

// leetcode 1323
int maximum69Number (int num)
{
    char str[5];

    sprintf(str, "%d", num);

    for (int i = 0; i < (int)strlen(str); i++) {
        if (str[i] == '6') {
            str[i] = '9';
            break;
        }
    }

    return atoi(str);
}