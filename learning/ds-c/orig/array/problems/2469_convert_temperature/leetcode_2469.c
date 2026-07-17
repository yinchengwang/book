#include <stdio.h>
#include <stdlib.h>

double* convertTemperature(double celsius, int* returnSize)
{
    double *res = (double *)malloc(sizeof(double) * 2);
    res[0] = celsius + 273.15;
    res[1] = celsius * 1.80 + 32.00;

    *returnSize = 2;

    return res;
}

int main()
{

    double celsius = 36.50;
    int returnSize = 0;
    double *res = convertTemperature(celsius, &returnSize);

    for (int i = 0; i < returnSize; i++) {
        printf("%.4lf\n", res[i]);
    }

    return 0;
}