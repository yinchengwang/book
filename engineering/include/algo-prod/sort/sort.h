#ifndef SORT_H
#define SORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*sort_compare_fn)(const void *lhs, const void *rhs);

typedef enum sort_algorithm {
	SORT_ALGORITHM_BUBBLE = 0,
	SORT_ALGORITHM_SELECTION = 1,
	SORT_ALGORITHM_INSERTION = 2,
	SORT_ALGORITHM_SHELL = 3,
	SORT_ALGORITHM_MERGE = 4,
	SORT_ALGORITHM_QUICK = 5,
	SORT_ALGORITHM_HEAP = 6,
	SORT_ALGORITHM_COUNTING_INT32 = 7,
	SORT_ALGORITHM_BUCKET_INT32 = 8,
	SORT_ALGORITHM_RADIX_INT32 = 9,
} sort_algorithm_t;

int sort_algorithm_is_valid(sort_algorithm_t algorithm);
int sort_dispatch(sort_algorithm_t algorithm,
				  void *base,
				  size_t count,
				  size_t element_size,
				  sort_compare_fn compare);

/* 稳定；平均/最坏 O(n^2)；适合教学、极小规模或基本有序数据。 */
int sort_bubble(void *base, size_t count, size_t element_size, sort_compare_fn compare);
/* 不稳定；平均/最坏 O(n^2)；适合交换成本低、实现需极简的场景。 */
int sort_selection(void *base, size_t count, size_t element_size, sort_compare_fn compare);
/* 稳定；平均/最坏 O(n^2)，最好 O(n)；适合小数组和近乎有序数据。 */
int sort_insertion(void *base, size_t count, size_t element_size, sort_compare_fn compare);
/* 不稳定；通常介于 O(n log n) 到 O(n^2)；适合中等规模、对原地排序有要求的场景。 */
int sort_shell(void *base, size_t count, size_t element_size, sort_compare_fn compare);
/* 稳定；平均/最坏 O(n log n)；适合需要稳定性和可预测性能的通用场景。 */
int sort_merge(void *base, size_t count, size_t element_size, sort_compare_fn compare);
/* 不稳定；平均 O(n log n)，最坏 O(n^2)；适合通用内存排序，平均性能通常最好。 */
int sort_quick(void *base, size_t count, size_t element_size, sort_compare_fn compare);
/* 不稳定；平均/最坏 O(n log n)；适合需要严格上界且希望原地排序的场景。 */
int sort_heap(void *base, size_t count, size_t element_size, sort_compare_fn compare);

/* 稳定；时间 O(n + k)，空间 O(k)；适合值域较小、整数密集分布的数据。 */
int sort_counting_int32(int32_t *array, size_t count);
/* 视桶内排序而定，此实现整体不保证稳定；平均接近 O(n + k)；适合分布较均匀的整数数据。 */
int sort_bucket_int32(int32_t *array, size_t count);
/* 稳定；时间 O(d(n + r))；适合定长整数、位数有限且不想做比较排序的场景。 */
int sort_radix_int32(int32_t *array, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* SORT_H */