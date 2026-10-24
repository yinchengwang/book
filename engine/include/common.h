#ifndef COMMON_H
#define COMMON_H

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int int_cmp(const void *lhs, const void *rhs)
{
    return (*(int *)lhs - *(int *)rhs);
}

static inline int int_array_cmp(const void *lhs, const void *rhs)
{
    return (*(int **)lhs)[0] - (*(int **)rhs)[0];
}

__forceinline void int_swap(int *lhs, int *rhs)
{
    int t = *lhs;
    *lhs = *rhs;
    *rhs = t;
}

#ifdef __cplusplus
}
#endif // extern "C"

#endif // COMMON_H