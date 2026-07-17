#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


#define STATUS_OK 0

#define TO_VOID_PTR(ptr) ((void *)(ptr))
#define TO_INT32_PTR(ptr) ((int32_t *)(ptr))
#define TO_UINT32_PTR(ptr) ((uint32_t *)(ptr))
#define TO_CHAR_PTR(ptr) ((char *)(ptr))

// 容器元素类型
typedef enum element_type {
    TYPE_INT,
    TYPE_CHAR,
    TYPE_DOUBLE,
    TYPE_STRUCT
} element_type_e;



#ifdef __cplusplus
}
#endif // extern "C"

#endif // DATA_TYPE_H