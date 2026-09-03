/*
 * array.c —— 动态数组（Vector）实现
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 *   ds_array_t
 *   ┌─────────────────────────────────────────┐
 *   │ data ──────────> [elem0][elem1]...[elemN-1][____未使用____]  │
 *   │ size       = N   <── 已使用的元素 ──> <── 预留容量 ──>       │
 *   │ capacity   = M   (M >= N)                                   │
 *   │ element_size                                                         │
 *   │ free_element                                                         │
 *   └─────────────────────────────────────────┘
 *
 * 扩容策略：当 size == capacity 时，新容量 = 旧容量 × 2（首次从默认值 8 开始）。
 * 这样 push_back 的均摊时间复杂度为 O(1)。
 */

#include <ds/array.h>

#include <stdlib.h>
#include <string.h>

#define DS_ARRAY_DEFAULT_CAPACITY 8u
#define DS_ARRAY_GROWTH_FACTOR 2u

struct ds_array {
    void  *data;
    size_t size;
    size_t capacity;
    size_t element_size;
    void (*free_element)(void *element);
};

/* ============================================================
 * 内部辅助：通过索引获取元素指针
 * ============================================================ */
static unsigned char *array_elem(ds_array_t *array, size_t index)
{
    return (unsigned char *)array->data + index * array->element_size;
}

static const unsigned char *array_elem_const(const ds_array_t *array, size_t index)
{
    return (const unsigned char *)array->data + index * array->element_size;
}

/*
 * ============================================================
 * 内部辅助：调整容量
 * ============================================================ */
static int array_set_capacity(ds_array_t *array, size_t new_capacity)
{
    void *new_data;

    if (new_capacity < array->size) {
        return -1;
    }
    if (new_capacity == 0u) {
        free(array->data);
        array->data = NULL;
        array->capacity = 0u;
        return 0;
    }

    new_data = realloc(array->data, new_capacity * array->element_size);
    if (!new_data) {
        return -1;
    }

    array->data = new_data;
    array->capacity = new_capacity;
    return 0;
}

/*
 * ============================================================
 * 扩容：翻倍策略
 *
 * 均摊分析：假设最终有 n 次 push_back，扩容发生在 1, 2, 4, 8, ... 次时。
 * 总拷贝次数 = 1 + 2 + 4 + ... + n/2 < n，均摊每次 O(1)。
 * ============================================================ */
static int array_grow(ds_array_t *array)
{
    size_t new_capacity;

    if (array->capacity == 0u) {
        new_capacity = DS_ARRAY_DEFAULT_CAPACITY;
    } else {
        new_capacity = array->capacity * DS_ARRAY_GROWTH_FACTOR;
        /* 防止溢出导致 capacity 不变 */
        if (new_capacity <= array->capacity) {
            new_capacity = array->capacity + 1u;
        }
    }

    return array_set_capacity(array, new_capacity);
}

/*
 * ============================================================
 * 移动元素块（用于 insert/remove）
 *
 * 将 [from_index, from_index+count) 的元素移动到 to_index 开始的位置。
 * 使用 memmove 而非 memcpy，因为源和目标可能重叠。
 * ============================================================ */
static void array_move_elements(ds_array_t *array, size_t from_index, size_t to_index, size_t count)
{
    if (count == 0u) {
        return;
    }
    memmove(
        array_elem(array, to_index),
        array_elem_const(array, from_index),
        count * array->element_size
    );
}

/* ============================================================
 * 公共 API
 * ============================================================ */

ds_array_t *ds_array_create(size_t element_size, size_t initial_capacity)
{
    ds_array_t *array;

    if (element_size == 0u) {
        return NULL;
    }
    if (initial_capacity == 0u) {
        initial_capacity = DS_ARRAY_DEFAULT_CAPACITY;
    }

    array = (ds_array_t *)calloc(1u, sizeof(ds_array_t));
    if (!array) {
        return NULL;
    }

    array->data = malloc(initial_capacity * element_size);
    if (!array->data) {
        free(array);
        return NULL;
    }

    array->capacity = initial_capacity;
    array->element_size = element_size;
    return array;
}

void ds_array_destroy(ds_array_t *array)
{
    if (!array) {
        return;
    }

    ds_array_clear(array);
    free(array->data);
    free(array);
}

int ds_array_get(const ds_array_t *array, size_t index, void *out)
{
    if (!array || index >= array->size || !out) {
        return -1;
    }

    memcpy(out, array_elem_const(array, index), array->element_size);
    return 0;
}

int ds_array_set(ds_array_t *array, size_t index, const void *element)
{
    if (!array || index >= array->size || !element) {
        return -1;
    }

    memcpy(array_elem(array, index), element, array->element_size);
    return 0;
}

const void *ds_array_get_ptr(const ds_array_t *array, size_t index)
{
    if (!array || index >= array->size) {
        return NULL;
    }

    return array_elem_const(array, index);
}

int ds_array_push_back(ds_array_t *array, const void *element)
{
    if (!array || !element) {
        return -1;
    }
    if (array->size >= array->capacity && array_grow(array) != 0) {
        return -1;
    }

    memcpy(array_elem(array, array->size), element, array->element_size);
    array->size += 1u;
    return 0;
}

int ds_array_pop_back(ds_array_t *array, void *out)
{
    if (!array || array->size == 0u) {
        return -1;
    }

    array->size -= 1u;
    if (out) {
        memcpy(out, array_elem(array, array->size), array->element_size);
    } else if (array->free_element) {
        array->free_element(array_elem(array, array->size));
    }
    return 0;
}

/*
 * ============================================================
 * 中间插入：将 [index, size) 的元素整体后移一位，空出 index 位置
 *
 *   插入前: [A][B][C][D]  (size=4)
 *                   ^index=2
 *   移动后: [A][B][_][C][D]
 *   写入后: [A][B][X][C][D]  (size=5)
 * ============================================================ */
int ds_array_insert(ds_array_t *array, size_t index, const void *element)
{
    if (!array || index > array->size || !element) {
        return -1;
    }
    if (array->size >= array->capacity && array_grow(array) != 0) {
        return -1;
    }

    /* 将 index 及之后的元素整体后移 1 位 */
    array_move_elements(array, index, index + 1u, array->size - index);

    memcpy(array_elem(array, index), element, array->element_size);
    array->size += 1u;
    return 0;
}

/*
 * ============================================================
 * 中间删除：将 [index+1, size) 的元素整体前移一位
 *
 *   删除前: [A][B][X][C][D]  (size=5)
 *                   ^index=2
 *   前移后: [A][B][C][D][D]  （最后的 D 是残留，size 减 1 后不可见）
 * ============================================================ */
int ds_array_remove_at(ds_array_t *array, size_t index, void *out)
{
    if (!array || index >= array->size) {
        return -1;
    }

    if (out) {
        memcpy(out, array_elem(array, index), array->element_size);
    } else if (array->free_element) {
        array->free_element(array_elem(array, index));
    }

    /* 将 index 之后的元素前移 1 位 */
    if (index + 1u < array->size) {
        array_move_elements(array, index + 1u, index, array->size - index - 1u);
    }

    array->size -= 1u;
    return 0;
}

int ds_array_reserve(ds_array_t *array, size_t capacity)
{
    if (!array) {
        return -1;
    }
    if (capacity <= array->capacity) {
        return 0;
    }

    return array_set_capacity(array, capacity);
}

int ds_array_shrink_to_fit(ds_array_t *array)
{
    if (!array) {
        return -1;
    }

    return array_set_capacity(array, array->size);
}

size_t ds_array_size(const ds_array_t *array)
{
    return array ? array->size : 0u;
}

size_t ds_array_capacity(const ds_array_t *array)
{
    return array ? array->capacity : 0u;
}

bool ds_array_empty(const ds_array_t *array)
{
    return !array || array->size == 0u;
}

void ds_array_clear(ds_array_t *array)
{
    if (!array) {
        return;
    }

    if (array->free_element) {
        for (size_t i = 0u; i < array->size; ++i) {
            array->free_element(array_elem(array, i));
        }
    }

    array->size = 0u;
}
