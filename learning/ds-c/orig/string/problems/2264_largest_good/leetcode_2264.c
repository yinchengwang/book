#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* largestGoodInteger(char* num) {
    int n = strlen(num);
    char* res = NULL;
    for (int i = 0; i < n - 2; ++i) {
        if (num[i] == num[i + 1] && num[i + 1] == num[i + 2]) {
            if (res == NULL || strncmp(&num[i], res, 3) > 0) {
                res = &num[i];
            }
        }
    }
    char* res2 = (char *)malloc(4);
    memset(res2, 0, 4);
    if (res == NULL) {
        return res2;
    }
    strncpy(res2, res, 3);
    return res2;
}

int main()
{
    char num[] = "6777133339";
    char *res = largestGoodInteger(num);
    printf("res is: %s.\n", res);

    char num1[] = "2300019";
    res = largestGoodInteger(num1);
    printf("res is: %s.\n", res);

    char num2[] = "42352338";
    res = largestGoodInteger(num2);
    printf("res is: %s.\n", res);

    return 0;
}