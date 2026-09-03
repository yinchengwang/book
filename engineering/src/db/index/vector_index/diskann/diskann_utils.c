/* diskann utility helpers */

#include "diskann_private.h"

/* scalar helpers */

int32_t diskann_max_i32(int32_t lhs, int32_t rhs)
{
    return lhs > rhs ? lhs : rhs;
}

int32_t diskann_min_i32(int32_t lhs, int32_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

bool diskann_float_is_valid(float value)
{
    return !isnan(value) && !isinf(value);
}

int32_t diskann_payload_bytes_per_block(int32_t page_size)
{
    return page_size - (int32_t)sizeof(diskann_block_header_t);
}

int32_t diskann_block_count_for_bytes(int32_t page_size, int32_t bytes)
{
    int32_t payload_size = diskann_payload_bytes_per_block(page_size);

    if (bytes <= 0 || payload_size <= 0) {
        return 0;
    }

    return (bytes + payload_size - 1) / payload_size;
}

long long diskann_block_offset(int32_t block_no, int32_t page_size)
{
    return (long long)block_no * (long long)page_size;
}

/* pq state helpers */

bool diskann_pq_enabled(const diskann_t *index)
{
    return index && index->quantization_params.enabled;
}

bool diskann_pq_ready(const diskann_t *index)
{
    return diskann_pq_enabled(index) && index->quantizer && quantizer_is_trained(index->quantizer);
}

float diskann_distance_between_ids(const diskann_t *index, int32_t lhs, int32_t rhs)
{
    return distance_compute_indexed(index->metric, index->vectors, index->dims, lhs, rhs);
}

float diskann_fast_l2_distance(const diskann_t *index, int32_t id_a, int32_t id_b)
{
    float dot;
    float sq_dist;
    int32_t d;
    const float *va;
    const float *vb;

    if (!index || id_a < 0 || id_a >= index->n_total || id_b < 0 || id_b >= index->n_total) {
        return FLT_MAX;
    }

    if (index->metric != DISTANCE_METRIC_L2_SQUARED || !index->norms) {
        return diskann_distance_between_ids(index, id_a, id_b);
    }

    va = &index->vectors[id_a * index->dims];
    vb = &index->vectors[id_b * index->dims];
    dot = 0.0f;
    for (d = 0; d < index->dims; ++d) {
        dot += va[d] * vb[d];
    }

    sq_dist = index->norms[id_a] + index->norms[id_b] - 2.0f * dot;
    if (sq_dist < 0.0f) {
        sq_dist = 0.0f;
    }
    return sq_dist;
}

float diskann_exact_distance_from_query(const diskann_t *index, const float *query, int32_t id)
{
    if (index->metric == DISTANCE_METRIC_L2_SQUARED && index->norms) {
        const float *v = &index->vectors[id * index->dims];
        float dot = 0.0f;
        float query_norm = 0.0f;
        float sq_dist;
        int32_t d;
        for (d = 0; d < index->dims; ++d) {
            dot += query[d] * v[d];
            query_norm += query[d] * query[d];
        }
        sq_dist = query_norm + index->norms[id] - 2.0f * dot;
        return sq_dist < 0.0f ? 0.0f : sq_dist;
    }
    return distance_compute_from_query(index->metric, query, index->vectors, index->dims, id);
}

int diskann_compare_scored(const void *lhs, const void *rhs)
{
    const diskann_scored_t *left = (const diskann_scored_t *)lhs;
    const diskann_scored_t *right = (const diskann_scored_t *)rhs;

    if (left->distance < right->distance) {
        return -1;
    }
    if (left->distance > right->distance) {
        return 1;
    }
    if (left->id < right->id) {
        return -1;
    }
    if (left->id > right->id) {
        return 1;
    }
    return 0;
}

/* storage capacity helpers */

int diskann_ensure_frozen_capacity(diskann_t *index)
{
    int32_t *new_frozen_points;
    int32_t old_capacity;
    int32_t i;

    if (!index) {
        return -1;
    }

    old_capacity = index->storage_params.frozen_point_count;
    if (old_capacity <= 0) {
        free(index->frozen_points);
        index->frozen_points = NULL;
        return 0;
    }

    new_frozen_points = (int32_t *)realloc(index->frozen_points, (size_t)old_capacity * sizeof(int32_t));
    if (!new_frozen_points) {
        return -1;
    }
    if (!index->frozen_points) {
        for (i = 0; i < old_capacity; ++i) {
            new_frozen_points[i] = -1;
        }
    }
    index->frozen_points = new_frozen_points;
    return 0;
}

int diskann_ensure_vector_capacity(diskann_t *index, int32_t target)
{
    float *new_vectors;
    uint8_t *new_codes;
    uint8_t *new_deleted;
    int32_t *new_neighbors;
    int32_t *new_neighbor_counts;
    int32_t old_capacity;
    int32_t new_capacity;
    size_t old_neighbor_slots;
    size_t new_neighbor_slots;
    size_t old_code_bytes;
    size_t new_code_bytes;

    if (!index || target < 0) {
        return -1;
    }
    if (target <= index->capacity) {
        return 0;
    }

    old_capacity = index->capacity;
    new_capacity = index->capacity > 0 ? index->capacity : 8;
    while (new_capacity < target) {
        if (new_capacity > INT32_MAX / 2) {
            return -1;
        }
        new_capacity *= 2;
    }

    new_vectors = (float *)realloc(index->vectors, (size_t)new_capacity * (size_t)index->dims * sizeof(float));
    if (!new_vectors) {
        return -1;
    }

    {
        float *new_norms = (float *)realloc(index->norms, (size_t)new_capacity * sizeof(float));
        if (!new_norms) {
            free(new_vectors);
            return -1;
        }
        memset(new_norms + old_capacity, 0, (size_t)(new_capacity - old_capacity) * sizeof(float));
        index->norms = new_norms;
    }

    new_deleted = (uint8_t *)realloc(index->deleted, (size_t)new_capacity * sizeof(uint8_t));
    if (!new_deleted) {
        free(new_vectors);
        return -1;
    }

    new_neighbors = (int32_t *)realloc(index->neighbors,
                                       (size_t)new_capacity * (size_t)index->index_size * sizeof(int32_t));
    if (!new_neighbors) {
        free(new_vectors);
        free(new_deleted);
        return -1;
    }

    new_neighbor_counts = (int32_t *)realloc(index->neighbor_counts, (size_t)new_capacity * sizeof(int32_t));
    if (!new_neighbor_counts) {
        free(new_vectors);
        free(new_deleted);
        free(new_neighbors);
        return -1;
    }

    old_code_bytes = (size_t)old_capacity * (size_t)index->pq_code_size;
    new_code_bytes = (size_t)new_capacity * (size_t)index->pq_code_size;
    new_codes = NULL;
    if (index->pq_code_size > 0) {
        new_codes = (uint8_t *)realloc(index->codes, new_code_bytes);
        if (!new_codes) {
            free(new_vectors);
            free(new_deleted);
            free(new_neighbors);
            free(new_neighbor_counts);
            return -1;
        }
    }

    old_neighbor_slots = (size_t)old_capacity * (size_t)index->index_size;
    new_neighbor_slots = (size_t)new_capacity * (size_t)index->index_size;

    index->vectors = new_vectors;
    index->deleted = new_deleted;
    index->neighbors = new_neighbors;
    index->neighbor_counts = new_neighbor_counts;
    index->codes = new_codes;

    memset(index->deleted + old_capacity, 0, (size_t)(new_capacity - old_capacity) * sizeof(uint8_t));
    memset(index->neighbor_counts + old_capacity, 0, (size_t)(new_capacity - old_capacity) * sizeof(int32_t));
    if (new_neighbor_slots > old_neighbor_slots) {
        memset(index->neighbors + old_neighbor_slots, -1, (new_neighbor_slots - old_neighbor_slots) * sizeof(int32_t));
    }
    if (new_code_bytes > old_code_bytes && index->codes) {
        memset(index->codes + old_code_bytes, 0, new_code_bytes - old_code_bytes);
    }

    index->capacity = new_capacity;
    return 0;
}

int diskann_ensure_code_storage(diskann_t *index)
{
    uint8_t *new_codes;
    size_t bytes;

    if (!index) {
        return -1;
    }

    if (index->pq_code_size <= 0) {
        free(index->codes);
        index->codes = NULL;
        return 0;
    }

    bytes = (size_t)diskann_max_i32(index->capacity, 1) * (size_t)index->pq_code_size;
    new_codes = (uint8_t *)realloc(index->codes, bytes);
    if (!new_codes) {
        return -1;
    }
    if (!index->codes) {
        memset(new_codes, 0, bytes);
    }
    index->codes = new_codes;
    return 0;
}

/* graph state helpers */

void diskann_reset_graph(diskann_t *index)
{
    size_t total_slots;

    if (!index || !index->neighbors || !index->neighbor_counts) {
        return;
    }

    total_slots = (size_t)index->capacity * (size_t)index->index_size;
    memset(index->neighbors, -1, total_slots * sizeof(int32_t));
    memset(index->neighbor_counts, 0, (size_t)index->capacity * sizeof(int32_t));
}

int diskann_collect_active_ids(const diskann_t *index, int32_t **active_ids, int32_t *count)
{
    int32_t *ids;
    int32_t i;
    int32_t cursor;

    if (!index || !active_ids || !count) {
        return -1;
    }

    *active_ids = NULL;
    *count = 0;
    if (index->active_count <= 0) {
        return 0;
    }

    ids = (int32_t *)malloc((size_t)index->active_count * sizeof(int32_t));
    if (!ids) {
        return -1;
    }

    cursor = 0;
    for (i = 0; i < index->n_total; ++i) {
        if (!index->deleted[i]) {
            ids[cursor++] = i;
        }
    }

    *active_ids = ids;
    *count = cursor;
    return 0;
}

/* active set helpers */

int32_t diskann_choose_entry_point_from_active(const diskann_t *index, const int32_t *active_ids, int32_t count)
{
    float *centroid;
    float best_distance;
    int32_t best_id;
    int32_t i;
    int32_t d;

    if (!index || !active_ids || count <= 0) {
        return -1;
    }

    if (index->metric != DISTANCE_METRIC_L2_SQUARED) {
        return active_ids[0];
    }

    centroid = (float *)calloc((size_t)index->dims, sizeof(float));
    if (!centroid) {
        return active_ids[0];
    }

    for (i = 0; i < count; ++i) {
        const float *vector = &index->vectors[active_ids[i] * index->dims];
        for (d = 0; d < index->dims; ++d) {
            centroid[d] += vector[d];
        }
    }

    for (d = 0; d < index->dims; ++d) {
        centroid[d] /= (float)count;
    }

    best_id = active_ids[0];
    best_distance = diskann_exact_distance_from_query(index, centroid, best_id);
    for (i = 1; i < count; ++i) {
        float current = diskann_exact_distance_from_query(index, centroid, active_ids[i]);
        if (current < best_distance) {
            best_distance = current;
            best_id = active_ids[i];
        }
    }

    free(centroid);
    return best_id;
}

int diskann_rebuild_frozen_points(diskann_t *index)
{
    int32_t *active_ids;
    int32_t active_count;
    int32_t target_count;
    int32_t used;
    int32_t i;

    if (!index) {
        return -1;
    }

    if (diskann_ensure_frozen_capacity(index) != 0) {
        return -1;
    }

    for (i = 0; i < index->storage_params.frozen_point_count; ++i) {
        index->frozen_points[i] = -1;
    }
    index->entry_point = -1;

    target_count = diskann_min_i32(index->active_count, diskann_max_i32(index->storage_params.frozen_point_count, 0));
    if (target_count <= 0 || index->active_count <= 0) {
        return 0;
    }

    if (diskann_collect_active_ids(index, &active_ids, &active_count) != 0) {
        return -1;
    }

    index->entry_point = diskann_choose_entry_point_from_active(index, active_ids, active_count);
    index->frozen_points[0] = index->entry_point;
    used = 1;

    for (i = 0; i < active_count && used < target_count; ++i) {
        int32_t sample_idx = (int32_t)(((int64_t)used * (int64_t)active_count) / (int64_t)target_count);
        int32_t candidate;
        int32_t j;
        bool duplicate = false;

        if (sample_idx >= active_count) {
            sample_idx = active_count - 1;
        }
        candidate = active_ids[sample_idx];
        for (j = 0; j < used; ++j) {
            if (index->frozen_points[j] == candidate) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            index->frozen_points[used++] = candidate;
        }
    }

    for (i = 0; i < active_count && used < target_count; ++i) {
        int32_t candidate = active_ids[i];
        int32_t j;
        bool duplicate = false;

        for (j = 0; j < used; ++j) {
            if (index->frozen_points[j] == candidate) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            index->frozen_points[used++] = candidate;
        }
    }

    free(active_ids);
    return 0;
}

/* ── 二叉最小堆操作 ── */
void diskann_heap_push(diskann_heap_t *heap, diskann_scored_t scored)
{
    int32_t i;

    if (!heap || !heap->data || heap->size >= heap->capacity) {
        return;
    }

    /* 上滤：新元素放在末尾，逐层与父节点比较交换 */
    i = heap->size;
    heap->data[i] = scored;
    while (i > 0) {
        int32_t parent = (i - 1) / 2;
        if (heap->data[parent].distance <= heap->data[i].distance) {
            break;
        }
        {
            diskann_scored_t tmp = heap->data[parent];
            heap->data[parent] = heap->data[i];
            heap->data[i] = tmp;
        }
        i = parent;
    }
    heap->size += 1;
}

diskann_scored_t diskann_heap_pop(diskann_heap_t *heap)
{
    diskann_scored_t result;
    int32_t i;

    result.id = -1;
    result.distance = FLT_MAX;
    result.expanded = false;

    if (!heap || !heap->data || heap->size <= 0) {
        return result;
    }

    result = heap->data[0];
    heap->size -= 1;
    if (heap->size > 0) {
        heap->data[0] = heap->data[heap->size];
        /* 下滤：将新堆顶与子节点中较小者交换 */
        i = 0;
        while (1) {
            int32_t left = 2 * i + 1;
            int32_t right = 2 * i + 2;
            int32_t smallest = i;
            int32_t size = heap->size;

            if (left < size && heap->data[left].distance < heap->data[smallest].distance) {
                smallest = left;
            }
            if (right < size && heap->data[right].distance < heap->data[smallest].distance) {
                smallest = right;
            }
            if (smallest == i) {
                break;
            }
            {
                diskann_scored_t tmp = heap->data[i];
                heap->data[i] = heap->data[smallest];
                heap->data[smallest] = tmp;
            }
            i = smallest;
        }
    }

    return result;
}

diskann_scored_t diskann_heap_peek(const diskann_heap_t *heap)
{
    diskann_scored_t result;
    result.id = -1;
    result.distance = FLT_MAX;
    result.expanded = false;

    if (heap && heap->data && heap->size > 0) {
        return heap->data[0];
    }
    return result;
}

void diskann_heap_skip_expanded(diskann_heap_t *heap)
{
    if (!heap || !heap->data) {
        return;
    }

    while (heap->size > 0 && heap->data[0].expanded) {
        diskann_heap_pop(heap);
    }
}

void diskann_heapify(diskann_heap_t *heap)
{
    int32_t i;

    if (!heap || !heap->data || heap->size <= 0) {
        return;
    }

    /* 自底向上：从最后一个非叶节点开始逐一下滤 */
    for (i = heap->size / 2 - 1; i >= 0; --i) {
        int32_t cur = i;
        while (1) {
            int32_t left = 2 * cur + 1;
            int32_t right = 2 * cur + 2;
            int32_t smallest = cur;
            int32_t size = heap->size;

            if (left < size && heap->data[left].distance < heap->data[smallest].distance) {
                smallest = left;
            }
            if (right < size && heap->data[right].distance < heap->data[smallest].distance) {
                smallest = right;
            }
            if (smallest == cur) {
                break;
            }
            {
                diskann_scored_t tmp = heap->data[cur];
                heap->data[cur] = heap->data[smallest];
                heap->data[smallest] = tmp;
            }
            cur = smallest;
        }
    }
}
