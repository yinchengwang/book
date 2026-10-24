#include <algo-prod/sort/sort.h>

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct quick_sort_range {
    size_t left;
    size_t right;
} quick_sort_range_t;

static unsigned char *sort_element_at(void *base, size_t index, size_t element_size)
{
    return (unsigned char *)base + index * element_size;
}

static int sort_validate_generic(void *base, size_t count, size_t element_size, sort_compare_fn compare)
{
    if ((!base && count > 0u) || element_size == 0u || !compare) {
        return -1;
    }
    return 0;
}

static int sort_validate_int32(int32_t *array, size_t count)
{
    if (!array && count > 0u) {
        return -1;
    }
    return 0;
}

static void sort_swap_bytes(void *lhs, void *rhs, size_t element_size)
{
    unsigned char *left = (unsigned char *)lhs;
    unsigned char *right = (unsigned char *)rhs;
    size_t index;

    if (lhs == rhs) {
        return;
    }

    for (index = 0u; index < element_size; ++index) {
        unsigned char temp = left[index];
        left[index] = right[index];
        right[index] = temp;
    }
}

static void sort_copy_bytes(void *dst, const void *src, size_t element_size)
{
    memcpy(dst, src, element_size);
}

static void sort_heap_sift_down(void *base,
                                size_t start,
                                size_t end,
                                size_t element_size,
                                sort_compare_fn compare)
{
    size_t root = start;

    while (true) {
        size_t child = root * 2u + 1u;
        size_t swap_index = root;

        if (child > end) {
            return;
        }
        if (compare(sort_element_at(base, swap_index, element_size),
                    sort_element_at(base, child, element_size)) < 0) {
            swap_index = child;
        }
        if (child + 1u <= end &&
            compare(sort_element_at(base, swap_index, element_size),
                    sort_element_at(base, child + 1u, element_size)) < 0) {
            swap_index = child + 1u;
        }
        if (swap_index == root) {
            return;
        }

        sort_swap_bytes(sort_element_at(base, root, element_size),
                        sort_element_at(base, swap_index, element_size),
                        element_size);
        root = swap_index;
    }
}

static int sort_merge_recursive(unsigned char *base,
                                unsigned char *temp,
                                size_t left,
                                size_t right,
                                size_t element_size,
                                sort_compare_fn compare)
{
    size_t mid;
    size_t left_index;
    size_t right_index;
    size_t write_index;

    if (right - left <= 1u) {
        return 0;
    }

    mid = left + (right - left) / 2u;
    if (sort_merge_recursive(base, temp, left, mid, element_size, compare) != 0) {
        return -1;
    }
    if (sort_merge_recursive(base, temp, mid, right, element_size, compare) != 0) {
        return -1;
    }

    left_index = left;
    right_index = mid;
    write_index = left;
    while (left_index < mid && right_index < right) {
        if (compare(base + left_index * element_size,
                    base + right_index * element_size) <= 0) {
            sort_copy_bytes(temp + write_index * element_size,
                            base + left_index * element_size,
                            element_size);
            left_index += 1u;
        } else {
            sort_copy_bytes(temp + write_index * element_size,
                            base + right_index * element_size,
                            element_size);
            right_index += 1u;
        }
        write_index += 1u;
    }

    while (left_index < mid) {
        sort_copy_bytes(temp + write_index * element_size,
                        base + left_index * element_size,
                        element_size);
        left_index += 1u;
        write_index += 1u;
    }
    while (right_index < right) {
        sort_copy_bytes(temp + write_index * element_size,
                        base + right_index * element_size,
                        element_size);
        right_index += 1u;
        write_index += 1u;
    }

    memcpy(base + left * element_size,
           temp + left * element_size,
           (right - left) * element_size);
    return 0;
}

static size_t sort_quick_partition(void *base,
                                   size_t left,
                                   size_t right,
                                   size_t element_size,
                                   sort_compare_fn compare)
{
    size_t pivot_index = left + (right - left) / 2u;
    size_t store_index = left;
    size_t index;

    sort_swap_bytes(sort_element_at(base, pivot_index, element_size),
                    sort_element_at(base, right, element_size),
                    element_size);

    for (index = left; index < right; ++index) {
        if (compare(sort_element_at(base, index, element_size),
                    sort_element_at(base, right, element_size)) < 0) {
            sort_swap_bytes(sort_element_at(base, index, element_size),
                            sort_element_at(base, store_index, element_size),
                            element_size);
            store_index += 1u;
        }
    }

    sort_swap_bytes(sort_element_at(base, store_index, element_size),
                    sort_element_at(base, right, element_size),
                    element_size);
    return store_index;
}

static void sort_insertion_int32(int32_t *array, size_t count)
{
    size_t index;

    for (index = 1u; index < count; ++index) {
        int32_t value = array[index];
        size_t position = index;

        while (position > 0u && array[position - 1u] > value) {
            array[position] = array[position - 1u];
            position -= 1u;
        }
        array[position] = value;
    }
}

int sort_algorithm_is_valid(sort_algorithm_t algorithm)
{
    return algorithm >= SORT_ALGORITHM_BUBBLE && algorithm <= SORT_ALGORITHM_RADIX_INT32;
}

int sort_dispatch(sort_algorithm_t algorithm,
                  void *base,
                  size_t count,
                  size_t element_size,
                  sort_compare_fn compare)
{
    if (!sort_algorithm_is_valid(algorithm)) {
        return -1;
    }

    switch (algorithm) {
    case SORT_ALGORITHM_BUBBLE:
        return sort_bubble(base, count, element_size, compare);
    case SORT_ALGORITHM_SELECTION:
        return sort_selection(base, count, element_size, compare);
    case SORT_ALGORITHM_INSERTION:
        return sort_insertion(base, count, element_size, compare);
    case SORT_ALGORITHM_SHELL:
        return sort_shell(base, count, element_size, compare);
    case SORT_ALGORITHM_MERGE:
        return sort_merge(base, count, element_size, compare);
    case SORT_ALGORITHM_QUICK:
        return sort_quick(base, count, element_size, compare);
    case SORT_ALGORITHM_HEAP:
        return sort_heap(base, count, element_size, compare);
    case SORT_ALGORITHM_COUNTING_INT32:
        if (element_size != sizeof(int32_t)) {
            return -1;
        }
        return sort_counting_int32((int32_t *)base, count);
    case SORT_ALGORITHM_BUCKET_INT32:
        if (element_size != sizeof(int32_t)) {
            return -1;
        }
        return sort_bucket_int32((int32_t *)base, count);
    case SORT_ALGORITHM_RADIX_INT32:
        if (element_size != sizeof(int32_t)) {
            return -1;
        }
        return sort_radix_int32((int32_t *)base, count);
    default:
        return -1;
    }
}

int sort_bubble(void *base, size_t count, size_t element_size, sort_compare_fn compare)
{
    size_t pass;
    size_t index;

    if (sort_validate_generic(base, count, element_size, compare) != 0) {
        return -1;
    }

    for (pass = count; pass > 1u; --pass) {
        bool swapped = false;

        for (index = 1u; index < pass; ++index) {
            if (compare(sort_element_at(base, index - 1u, element_size),
                        sort_element_at(base, index, element_size)) > 0) {
                sort_swap_bytes(sort_element_at(base, index - 1u, element_size),
                                sort_element_at(base, index, element_size),
                                element_size);
                swapped = true;
            }
        }
        if (!swapped) {
            break;
        }
    }

    return 0;
}

int sort_selection(void *base, size_t count, size_t element_size, sort_compare_fn compare)
{
    size_t index;

    if (sort_validate_generic(base, count, element_size, compare) != 0) {
        return -1;
    }

    for (index = 0u; index + 1u < count; ++index) {
        size_t min_index = index;
        size_t cursor;

        for (cursor = index + 1u; cursor < count; ++cursor) {
            if (compare(sort_element_at(base, cursor, element_size),
                        sort_element_at(base, min_index, element_size)) < 0) {
                min_index = cursor;
            }
        }

        if (min_index != index) {
            sort_swap_bytes(sort_element_at(base, index, element_size),
                            sort_element_at(base, min_index, element_size),
                            element_size);
        }
    }

    return 0;
}

int sort_insertion(void *base, size_t count, size_t element_size, sort_compare_fn compare)
{
    unsigned char *temp;
    size_t index;

    if (sort_validate_generic(base, count, element_size, compare) != 0) {
        return -1;
    }
    if (count < 2u) {
        return 0;
    }

    temp = (unsigned char *)malloc(element_size);
    if (!temp) {
        return -1;
    }

    for (index = 1u; index < count; ++index) {
        size_t position = index;

        sort_copy_bytes(temp, sort_element_at(base, index, element_size), element_size);
        while (position > 0u && compare(sort_element_at(base, position - 1u, element_size), temp) > 0) {
            sort_copy_bytes(sort_element_at(base, position, element_size),
                            sort_element_at(base, position - 1u, element_size),
                            element_size);
            position -= 1u;
        }
        sort_copy_bytes(sort_element_at(base, position, element_size), temp, element_size);
    }

    free(temp);
    return 0;
}

int sort_shell(void *base, size_t count, size_t element_size, sort_compare_fn compare)
{
    unsigned char *temp;
    size_t gap;

    if (sort_validate_generic(base, count, element_size, compare) != 0) {
        return -1;
    }
    if (count < 2u) {
        return 0;
    }

    temp = (unsigned char *)malloc(element_size);
    if (!temp) {
        return -1;
    }

    for (gap = count / 2u; gap > 0u; gap /= 2u) {
        size_t index;

        for (index = gap; index < count; ++index) {
            size_t position = index;

            sort_copy_bytes(temp, sort_element_at(base, index, element_size), element_size);
            while (position >= gap && compare(sort_element_at(base, position - gap, element_size), temp) > 0) {
                sort_copy_bytes(sort_element_at(base, position, element_size),
                                sort_element_at(base, position - gap, element_size),
                                element_size);
                position -= gap;
            }
            sort_copy_bytes(sort_element_at(base, position, element_size), temp, element_size);
        }
        if (gap == 1u) {
            break;
        }
    }

    free(temp);
    return 0;
}

int sort_merge(void *base, size_t count, size_t element_size, sort_compare_fn compare)
{
    unsigned char *temp;

    if (sort_validate_generic(base, count, element_size, compare) != 0) {
        return -1;
    }
    if (count < 2u) {
        return 0;
    }

    temp = (unsigned char *)malloc(count * element_size);
    if (!temp) {
        return -1;
    }

    if (sort_merge_recursive((unsigned char *)base, temp, 0u, count, element_size, compare) != 0) {
        free(temp);
        return -1;
    }

    free(temp);
    return 0;
}

int sort_quick(void *base, size_t count, size_t element_size, sort_compare_fn compare)
{
    quick_sort_range_t *stack;
    size_t top = 0u;

    if (sort_validate_generic(base, count, element_size, compare) != 0) {
        return -1;
    }
    if (count < 2u) {
        return 0;
    }

    stack = (quick_sort_range_t *)malloc(count * sizeof(quick_sort_range_t));
    if (!stack) {
        return -1;
    }

    stack[top].left = 0u;
    stack[top].right = count - 1u;
    top += 1u;

    while (top > 0u) {
        quick_sort_range_t range = stack[--top];

        if (range.left >= range.right) {
            continue;
        }

        {
            size_t pivot = sort_quick_partition(base, range.left, range.right, element_size, compare);

            if (pivot > range.left) {
                stack[top].left = range.left;
                stack[top].right = pivot - 1u;
                top += 1u;
            }
            if (pivot + 1u < range.right) {
                stack[top].left = pivot + 1u;
                stack[top].right = range.right;
                top += 1u;
            }
        }
    }

    free(stack);
    return 0;
}

int sort_heap(void *base, size_t count, size_t element_size, sort_compare_fn compare)
{
    size_t start;
    size_t end;

    if (sort_validate_generic(base, count, element_size, compare) != 0) {
        return -1;
    }
    if (count < 2u) {
        return 0;
    }

    for (start = count / 2u; start > 0u; --start) {
        sort_heap_sift_down(base, start - 1u, count - 1u, element_size, compare);
    }
    for (end = count - 1u; end > 0u; --end) {
        sort_swap_bytes(sort_element_at(base, 0u, element_size),
                        sort_element_at(base, end, element_size),
                        element_size);
        sort_heap_sift_down(base, 0u, end - 1u, element_size, compare);
    }

    return 0;
}

int sort_counting_int32(int32_t *array, size_t count)
{
    int32_t min_value;
    int32_t max_value;
    int64_t range64;
    size_t range;
    size_t *counts;
    size_t index;
    size_t write_index;

    if (sort_validate_int32(array, count) != 0) {
        return -1;
    }
    if (count < 2u) {
        return 0;
    }

    min_value = array[0];
    max_value = array[0];
    for (index = 1u; index < count; ++index) {
        if (array[index] < min_value) {
            min_value = array[index];
        }
        if (array[index] > max_value) {
            max_value = array[index];
        }
    }

    range64 = (int64_t)max_value - (int64_t)min_value + 1;
    if (range64 <= 0 || (uint64_t)range64 > SIZE_MAX / sizeof(size_t)) {
        return -1;
    }

    range = (size_t)range64;
    counts = (size_t *)calloc(range, sizeof(size_t));
    if (!counts) {
        return -1;
    }

    for (index = 0u; index < count; ++index) {
        counts[(size_t)((int64_t)array[index] - (int64_t)min_value)] += 1u;
    }

    write_index = 0u;
    for (index = 0u; index < range; ++index) {
        while (counts[index] > 0u) {
            array[write_index++] = (int32_t)((int64_t)min_value + (int64_t)index);
            counts[index] -= 1u;
        }
    }

    free(counts);
    return 0;
}

int sort_bucket_int32(int32_t *array, size_t count)
{
    int32_t min_value;
    int32_t max_value;
    int32_t **buckets;
    size_t *bucket_sizes;
    size_t *bucket_capacities;
    size_t bucket_count;
    size_t index;
    size_t write_index;

    if (sort_validate_int32(array, count) != 0) {
        return -1;
    }
    if (count < 2u) {
        return 0;
    }

    min_value = array[0];
    max_value = array[0];
    for (index = 1u; index < count; ++index) {
        if (array[index] < min_value) {
            min_value = array[index];
        }
        if (array[index] > max_value) {
            max_value = array[index];
        }
    }
    if (min_value == max_value) {
        return 0;
    }

    bucket_count = count;
    buckets = (int32_t **)calloc(bucket_count, sizeof(int32_t *));
    bucket_sizes = (size_t *)calloc(bucket_count, sizeof(size_t));
    bucket_capacities = (size_t *)calloc(bucket_count, sizeof(size_t));
    if (!buckets || !bucket_sizes || !bucket_capacities) {
        free(buckets);
        free(bucket_sizes);
        free(bucket_capacities);
        return -1;
    }

    for (index = 0u; index < count; ++index) {
        int64_t normalized = (int64_t)array[index] - (int64_t)min_value;
        int64_t range = (int64_t)max_value - (int64_t)min_value + 1;
        size_t bucket_index = (size_t)((normalized * (int64_t)(bucket_count - 1u)) / range);

        if (bucket_sizes[bucket_index] == bucket_capacities[bucket_index]) {
            size_t new_capacity = bucket_capacities[bucket_index] == 0u ? 4u : bucket_capacities[bucket_index] * 2u;
            int32_t *new_bucket = (int32_t *)realloc(buckets[bucket_index], new_capacity * sizeof(int32_t));

            if (!new_bucket) {
                size_t free_index;

                for (free_index = 0u; free_index < bucket_count; ++free_index) {
                    free(buckets[free_index]);
                }
                free(buckets);
                free(bucket_sizes);
                free(bucket_capacities);
                return -1;
            }

            buckets[bucket_index] = new_bucket;
            bucket_capacities[bucket_index] = new_capacity;
        }

        buckets[bucket_index][bucket_sizes[bucket_index]++] = array[index];
    }

    write_index = 0u;
    for (index = 0u; index < bucket_count; ++index) {
        if (bucket_sizes[index] > 1u) {
            sort_insertion_int32(buckets[index], bucket_sizes[index]);
        }
        if (bucket_sizes[index] > 0u) {
            memcpy(array + write_index, buckets[index], bucket_sizes[index] * sizeof(int32_t));
            write_index += bucket_sizes[index];
        }
        free(buckets[index]);
    }

    free(buckets);
    free(bucket_sizes);
    free(bucket_capacities);
    return 0;
}

int sort_radix_int32(int32_t *array, size_t count)
{
    int32_t *temp;
    size_t pass;
    size_t index;

    if (sort_validate_int32(array, count) != 0) {
        return -1;
    }
    if (count < 2u) {
        return 0;
    }

    temp = (int32_t *)malloc(count * sizeof(int32_t));
    if (!temp) {
        return -1;
    }

    for (pass = 0u; pass < 4u; ++pass) {
        size_t counts[256] = {0};
        size_t offsets[256];

        for (index = 0u; index < count; ++index) {
            uint32_t key = ((uint32_t)array[index] ^ 0x80000000u) >> (pass * 8u);
            counts[key & 0xffu] += 1u;
        }

        offsets[0] = 0u;
        for (index = 1u; index < 256u; ++index) {
            offsets[index] = offsets[index - 1u] + counts[index - 1u];
        }

        for (index = 0u; index < count; ++index) {
            uint32_t key = ((uint32_t)array[index] ^ 0x80000000u) >> (pass * 8u);
            size_t bucket = key & 0xffu;

            temp[offsets[bucket]++] = array[index];
        }

        memcpy(array, temp, count * sizeof(int32_t));
    }

    free(temp);
    return 0;
}