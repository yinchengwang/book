#ifndef COMPARE_H
#define COMPARE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*FaissIntMaxCmp)(int32_t, int32_t);
inline static bool faiss_int_max_cmp(int32_t a, int32_t b)
{
    return a > b;
}

typedef bool (*FaissFloatMaxCmp)(float, float);
inline static bool faiss_float_max_cmp(float a, float b)
{
    return a > b;
}


typedef bool (*FaissHeapMaxCmpFunc)(float, float, int32_t, int32_t);
inline static bool faiss_heap_max_cmp(float a1, float b1, int32_t a2, int32_t b2)
{
    return (a1 > b1) || ((a1 == b1) && (a2 > b2));
}

#ifdef __cplusplus
}
#endif // extern "C"

#endif // COMPARE_H