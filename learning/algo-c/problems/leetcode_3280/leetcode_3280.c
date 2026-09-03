#include<stdio.h>
#include<stdlib.h>

int toInt(char *s, int l, int r) {
    int res = 0;
    for (; l < r; l++) {
        res = res * 10 + s[l] - '0';
    }
    return res;
}

int binary(char *s, int l, int x) {
    int r = l;
    for (; x; x >>= 1) {
        s[r++] = '0' + (x & 1);
    }
    s[r] = '\0';
    // printf("%s\n", s);
    for (int i = l, j = r - 1; i < j; i++, j--) {
        char t = s[i];
        s[i] = s[j];
        s[j] = t;
    }

    return r;
}

char* convertDateToBinary(char* date) {
    int year = toInt(date, 0, 4);
    int month = toInt(date, 5, 7);
    int day = toInt(date, 8, 10);

    char *res = (char *)malloc(128);
    int i = binary(res, 0, year);
    res[i++] = '-';
    i = binary(res, i, month);
    res[i++] = '-';
    i = binary(res, i, day);
    res[i++] = '\0';
    return res;
}

int main()
{
    char *date = (char*)"2080-02-29";
    char *res = convertDateToBinary(date);
    printf("res = %s\n", res);

    return 0;
}