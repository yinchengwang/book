#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

int countKeyChanges(char* s)
{
    int res = 0;
    size_t len = strlen(s);
    for (int i = 0; i < len - 1; i++) {
        if (tolower(s[i]) != tolower(s[i + 1])) {
            res++;
        }
    }

    return res;
}

int main()
{
    char s[] = "aAbBcC";
    int res = countKeyChanges(s);
    printf("res is: %d.\n", res);

    char s1[] = "AaAaAaaA";
    res = countKeyChanges(s1);
    printf("res is: %d.\n", res);

    return 0;
}