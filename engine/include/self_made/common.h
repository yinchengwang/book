#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int orderSearch(int *nums, int numsLen, int target);

extern bool array2dSearch(int target, int **array, int arrayRowLen, int *arrayColLen);

extern int findPeakElement(int *nums, int numsLen);


#ifdef __cplusplus
}
#endif // extern "C"

#endif // COMMON_H