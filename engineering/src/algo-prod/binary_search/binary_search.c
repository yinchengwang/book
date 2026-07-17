#include <algo-prod/binary_search/binary_search.h>

static const unsigned char *binary_search_element_at(const void *base,
                                                     size_t index,
                                                     size_t element_size)
{
    return (const unsigned char *)base + index * element_size;
}

size_t binary_search_lower_bound(const void *base,
                                 size_t count,
                                 size_t element_size,
                                 const void *key,
                                 binary_search_compare_fn compare)
{
    size_t left;
    size_t right;

    if ((!base && count > 0u) || element_size == 0u || !key || !compare) {
        return count;
    }

    left = 0u;
    right = count;
    while (left < right) {
        size_t mid = left + (right - left) / 2u;
        const void *element = binary_search_element_at(base, mid, element_size);

        if (compare(element, key) < 0) {
            left = mid + 1u;
        } else {
            right = mid;
        }
    }

    return left;
}

size_t binary_search_upper_bound(const void *base,
                                 size_t count,
                                 size_t element_size,
                                 const void *key,
                                 binary_search_compare_fn compare)
{
    size_t left;
    size_t right;

    if ((!base && count > 0u) || element_size == 0u || !key || !compare) {
        return count;
    }

    left = 0u;
    right = count;
    while (left < right) {
        size_t mid = left + (right - left) / 2u;
        const void *element = binary_search_element_at(base, mid, element_size);

        if (compare(element, key) <= 0) {
            left = mid + 1u;
        } else {
            right = mid;
        }
    }

    return left;
}

bool binary_search(const void *base,
                   size_t count,
                   size_t element_size,
                   const void *key,
                   binary_search_compare_fn compare,
                   size_t *index)
{
    size_t position;

    if ((!base && count > 0u) || element_size == 0u || !key || !compare) {
        return false;
    }

    position = binary_search_lower_bound(base, count, element_size, key, compare);
    if (position >= count || compare(binary_search_element_at(base, position, element_size), key) != 0) {
        return false;
    }

    if (index) {
        *index = position;
    }
    return true;
}