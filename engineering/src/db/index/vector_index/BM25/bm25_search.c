#include "bm25_private.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

#define BM25_BMW_BLOCK_LOOKAHEAD 3

/* 获取当前微秒级时间戳。 */
static int64_t bm25_get_time_us(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (int64_t)(counter.QuadPart * 1000000LL / freq.QuadPart);
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + (int64_t)ts.tv_nsec / 1000LL;
#endif
}

typedef struct bm25_result_heap {
    bm25_scored_doc_t *items;
    int32_t size;
    int32_t capacity;
} bm25_result_heap_t;

typedef struct bm25_daat_term_state {
    const bm25_term_slot_t *slot;
    int32_t query_freq;
    int32_t cursor;
    int32_t filter_cursor;
} bm25_daat_term_state_t;

static float bm25_result_heap_threshold(const bm25_result_heap_t *heap)
{
    if (!heap || heap->capacity <= 0 || heap->size < heap->capacity) {
        return 0.0f;
    }
    return heap->items[0].score;
}

static int bm25_collect_query_terms(const bm25_t *index,
                                    int32_t query_term_count,
                                    const char *const *query_terms,
                                    bm25_term_freq_map_t *query_map)
{
    int32_t i;

    if (!query_map) {
        return -1;
    }
    memset(query_map, 0, sizeof(*query_map));
    if (!index || query_term_count < 0 || (query_term_count > 0 && !query_terms)) {
        return -1;
    }
    if (bm25_term_freq_map_init(query_map, query_term_count) != 0) {
        return -1;
    }

    for (i = 0; i < query_term_count; ++i) {
        char *normalized_term;
        uint64_t hash_value;

        if (!query_terms[i] || query_terms[i][0] == '\0') {
            continue;
        }

        normalized_term = NULL;
        if (dict_normalize_term(index->tokenizer, query_terms[i], &normalized_term) != 0) {
            bm25_term_freq_map_destroy(query_map);
            return -1;
        }
        if (!normalized_term) {
            continue;
        }

        hash_value = bm25_hash_term(normalized_term);
        if (bm25_term_freq_map_increment(query_map, normalized_term, hash_value) != 0) {
            bm25_term_freq_map_destroy(query_map);
            return -1;
        }
    }

    return 0;
}

static int bm25_result_is_better(float lhs_score, int32_t lhs_doc_id, float rhs_score, int32_t rhs_doc_id)
{
    if (lhs_score > rhs_score) {
        return 1;
    }
    if (lhs_score < rhs_score) {
        return 0;
    }
    return lhs_doc_id < rhs_doc_id;
}

static int bm25_result_is_worse(float lhs_score, int32_t lhs_doc_id, float rhs_score, int32_t rhs_doc_id)
{
    return bm25_result_is_better(rhs_score, rhs_doc_id, lhs_score, lhs_doc_id);
}

static void bm25_result_heap_swap(bm25_scored_doc_t *lhs, bm25_scored_doc_t *rhs)
{
    bm25_scored_doc_t temp = *lhs;

    *lhs = *rhs;
    *rhs = temp;
}

static void bm25_result_heap_sift_up(bm25_result_heap_t *heap, int32_t index)
{
    while (index > 0) {
        int32_t parent = (index - 1) / 2;

        if (!bm25_result_is_worse(heap->items[index].score,
                                  heap->items[index].doc_id,
                                  heap->items[parent].score,
                                  heap->items[parent].doc_id)) {
            break;
        }
        bm25_result_heap_swap(&heap->items[parent], &heap->items[index]);
        index = parent;
    }
}

static void bm25_result_heap_sift_down(bm25_result_heap_t *heap, int32_t index)
{
    while (true) {
        int32_t left = index * 2 + 1;
        int32_t right = left + 1;
        int32_t worst = index;

        if (left < heap->size &&
            bm25_result_is_worse(heap->items[left].score,
                                 heap->items[left].doc_id,
                                 heap->items[worst].score,
                                 heap->items[worst].doc_id)) {
            worst = left;
        }
        if (right < heap->size &&
            bm25_result_is_worse(heap->items[right].score,
                                 heap->items[right].doc_id,
                                 heap->items[worst].score,
                                 heap->items[worst].doc_id)) {
            worst = right;
        }
        if (worst == index) {
            break;
        }

        bm25_result_heap_swap(&heap->items[index], &heap->items[worst]);
        index = worst;
    }
}

static int bm25_result_heap_init(bm25_result_heap_t *heap, int32_t capacity)
{
    if (!heap || capacity <= 0) {
        return -1;
    }

    memset(heap, 0, sizeof(*heap));
    heap->items = (bm25_scored_doc_t *)malloc((size_t)capacity * sizeof(bm25_scored_doc_t));
    if (!heap->items) {
        return -1;
    }

    heap->capacity = capacity;
    return 0;
}

static void bm25_result_heap_destroy(bm25_result_heap_t *heap)
{
    if (!heap) {
        return;
    }

    free(heap->items);
    memset(heap, 0, sizeof(*heap));
}

static void bm25_result_heap_consider(bm25_result_heap_t *heap, int32_t doc_id, float score)
{
    if (!heap || heap->capacity <= 0) {
        return;
    }

    if (heap->size < heap->capacity) {
        heap->items[heap->size].doc_id = doc_id;
        heap->items[heap->size].score = score;
        bm25_result_heap_sift_up(heap, heap->size);
        heap->size += 1;
        return;
    }

    if (bm25_result_is_better(score, doc_id, heap->items[0].score, heap->items[0].doc_id)) {
        heap->items[0].doc_id = doc_id;
        heap->items[0].score = score;
        bm25_result_heap_sift_down(heap, 0);
    }
}

static int bm25_fill_outputs_from_heap(const bm25_result_heap_t *heap,
                                        int32_t k,
                                        float *scores,
                                        int32_t *doc_ids)
{
    bm25_scored_doc_t *ranked;
    int32_t limit;
    int32_t i;

    if (!heap || heap->size <= 0 || k <= 0) {
        return 0;
    }

    ranked = (bm25_scored_doc_t *)malloc((size_t)heap->size * sizeof(bm25_scored_doc_t));
    if (!ranked) {
        return -1;
    }

    memcpy(ranked, heap->items, (size_t)heap->size * sizeof(bm25_scored_doc_t));
    qsort(ranked, (size_t)heap->size, sizeof(bm25_scored_doc_t), bm25_compare_scored_docs_desc);

    limit = heap->size < k ? heap->size : k;
    for (i = 0; i < limit; ++i) {
        scores[i] = ranked[i].score;
        doc_ids[i] = ranked[i].doc_id;
    }

    free(ranked);
    return 0;
}

static int32_t bm25_effective_candidate_queue_size(const bm25_t *index, int32_t k)
{
    int32_t queue_size;

    if (!index) {
        return k;
    }

    queue_size = index->params.candidate_queue_size > 0 ? index->params.candidate_queue_size : k;
    if (queue_size < k) {
        queue_size = k;
    }
    return queue_size;
}

static bm25_search_algorithm_t bm25_pick_search_algorithm(const bm25_t *index,
                                                          const bm25_term_freq_map_t *query_map,
                                                          int32_t k)
{
    int32_t queue_size;

    if (!index) {
        return BM25_SEARCH_ALGORITHM_TAAT;
    }
    if (index->params.search_algorithm == BM25_SEARCH_ALGORITHM_TAAT ||
        index->params.search_algorithm == BM25_SEARCH_ALGORITHM_DAAT) {
        return index->params.search_algorithm;
    }

    queue_size = bm25_effective_candidate_queue_size(index, k);
    if (query_map && query_map->item_count <= 1) {
        return BM25_SEARCH_ALGORITHM_DAAT;
    }
    return queue_size <= index->params.daat_threshold ? BM25_SEARCH_ALGORITHM_DAAT : BM25_SEARCH_ALGORITHM_TAAT;
}

static int bm25_execute_taat(const bm25_t *index,
                             const bm25_term_freq_map_t *query_map,
                             int32_t k,
                             float *scores,
                             int32_t *doc_ids,
                             int32_t *hit_count)
{
    bm25_result_heap_t heap;
    bm25_search_stats_t *stats;
    float *accumulated_scores;
    uint8_t *seen_docs;
    int32_t *candidate_docs;
    int32_t local_hit_count = 0;
    int32_t queue_size;
    int32_t i;

    if (!index || !query_map || !hit_count) {
        return -1;
    }

    stats = (bm25_search_stats_t *)&index->last_search_stats;
    bm25_reset_search_stats(stats, BM25_SEARCH_ALGORITHM_TAAT);

    accumulated_scores = (float *)calloc((size_t)index->doc_count, sizeof(float));
    seen_docs = (uint8_t *)calloc((size_t)index->doc_count, sizeof(uint8_t));
    candidate_docs = index->doc_count > 0 ? (int32_t *)malloc((size_t)index->doc_count * sizeof(int32_t)) : NULL;
    if ((index->doc_count > 0 && (!accumulated_scores || !seen_docs || !candidate_docs))) {
        free(accumulated_scores);
        free(seen_docs);
        free(candidate_docs);
        return -1;
    }

    for (i = 0; i < query_map->item_count; ++i) {
        const bm25_term_freq_entry_t *query_entry = query_map->items[i];
        int32_t term_slot_index = bm25_find_term_slot_index(index, query_entry->term, query_entry->hash_value);
        const bm25_term_slot_t *term_slot = bm25_get_term_slot(index, term_slot_index);
        int32_t posting_index;

        if (!term_slot) {
            continue;
        }

        for (posting_index = 0; posting_index < term_slot->posting_count; ++posting_index) {
            const bm25_posting_t *posting = &term_slot->postings[posting_index];
            float score_delta = bm25_calculate_term_score(index,
                                                          term_slot->doc_freq,
                                                          posting->term_freq,
                                                          index->doc_lengths[posting->doc_id]);

            if (!seen_docs[posting->doc_id]) {
                seen_docs[posting->doc_id] = 1;
                candidate_docs[local_hit_count] = posting->doc_id;
                local_hit_count += 1;
            }
            accumulated_scores[posting->doc_id] += score_delta * (float)query_entry->freq;
        }
    }

    stats->candidate_count = local_hit_count;
    stats->heap_compare_count = local_hit_count;
    *hit_count = local_hit_count;
    if (local_hit_count == 0) {
        free(accumulated_scores);
        free(seen_docs);
        free(candidate_docs);
        return 0;
    }

    queue_size = bm25_effective_candidate_queue_size(index, k);
    if (bm25_result_heap_init(&heap, queue_size) != 0) {
        free(accumulated_scores);
        free(seen_docs);
        free(candidate_docs);
        return -1;
    }

    for (i = 0; i < local_hit_count; ++i) {
        int32_t doc_id = candidate_docs[i];

        bm25_result_heap_consider(&heap, doc_id, accumulated_scores[doc_id]);
    }
    stats->scored_doc_count = local_hit_count;

    if (bm25_fill_outputs_from_heap(&heap, k, scores, doc_ids) != 0) {
        bm25_result_heap_destroy(&heap);
        free(accumulated_scores);
        free(seen_docs);
        free(candidate_docs);
        return -1;
    }
    bm25_result_heap_destroy(&heap);
    free(accumulated_scores);
    free(seen_docs);
    free(candidate_docs);
    return 0;
}

static int bm25_count_query_hits(const bm25_t *index,
                                 const bm25_term_freq_map_t *query_map,
                                 int32_t *hit_count)
{
    uint8_t *seen_docs;
    int32_t total_hits = 0;
    int32_t i;

    if (!index || !query_map || !hit_count) {
        return -1;
    }
    if (index->doc_count <= 0) {
        *hit_count = 0;
        return 0;
    }

    seen_docs = (uint8_t *)calloc((size_t)index->doc_count, sizeof(uint8_t));
    if (!seen_docs) {
        return -1;
    }

    for (i = 0; i < query_map->item_count; ++i) {
        const bm25_term_freq_entry_t *query_entry = query_map->items[i];
        int32_t term_slot_index = bm25_find_term_slot_index(index, query_entry->term, query_entry->hash_value);
        const bm25_term_slot_t *term_slot = bm25_get_term_slot(index, term_slot_index);
        int32_t posting_index;

        if (!term_slot) {
            continue;
        }

        for (posting_index = 0; posting_index < term_slot->posting_count; ++posting_index) {
            int32_t doc_id = term_slot->postings[posting_index].doc_id;

            if (!seen_docs[doc_id]) {
                seen_docs[doc_id] = 1;
                total_hits += 1;
            }
        }
    }

    free(seen_docs);
    *hit_count = total_hits;
    return 0;
}

static void bm25_daat_sync_filter_cursor(bm25_daat_term_state_t *state)
{
    while (state->filter_cursor < state->slot->posting_filter_count &&
           state->cursor >= state->slot->posting_filters[state->filter_cursor].end_offset) {
        state->filter_cursor += 1;
    }
}

static int32_t bm25_daat_current_doc_id(const bm25_daat_term_state_t *state)
{
    if (!state || !state->slot || state->cursor < 0 || state->cursor >= state->slot->posting_count) {
        return INT_MAX;
    }
    return state->slot->postings[state->cursor].doc_id;
}

static float bm25_daat_current_block_upper_bound(const bm25_daat_term_state_t *state)
{
    if (!state || !state->slot || state->cursor >= state->slot->posting_count ||
        state->filter_cursor < 0 || state->filter_cursor >= state->slot->posting_filter_count) {
        return 0.0f;
    }
    return state->slot->posting_filters[state->filter_cursor].max_score * (float)state->query_freq;
}

static int32_t bm25_daat_candidate_block_target_doc(const bm25_daat_term_state_t *state, int32_t block_offset)
{
    int32_t filter_index;

    if (!state || !state->slot || block_offset < 0) {
        return INT_MAX;
    }

    filter_index = state->filter_cursor + block_offset;
    if (filter_index < 0 || filter_index >= state->slot->posting_filter_count) {
        return INT_MAX;
    }

    if (state->slot->posting_filters[filter_index].last_doc_id == INT_MAX) {
        return INT_MAX;
    }
    if (state->slot->posting_filters[filter_index].last_doc_id >= INT_MAX - 1) {
        return INT_MAX;
    }
    return state->slot->posting_filters[filter_index].last_doc_id + 1;
}

static float bm25_daat_range_block_upper_bound(const bm25_daat_term_state_t *state, int32_t target_doc)
{
    float upper_bound = 0.0f;
    int32_t filter_index;

    if (!state || !state->slot || target_doc == INT_MAX || state->cursor >= state->slot->posting_count) {
        return 0.0f;
    }

    for (filter_index = state->filter_cursor; filter_index < state->slot->posting_filter_count; ++filter_index) {
        const bm25_posting_filter_t *filter = &state->slot->posting_filters[filter_index];
        float block_score;

        if (filter->start_offset >= state->slot->posting_count) {
            break;
        }
        if (filter->last_doc_id >= target_doc) {
            break;
        }

        block_score = filter->max_score * (float)state->query_freq;
        if (block_score > upper_bound) {
            upper_bound = block_score;
        }
    }

    return upper_bound;
}

static int bm25_daat_state_precedes(const bm25_daat_term_state_t *lhs, const bm25_daat_term_state_t *rhs)
{
    int32_t lhs_doc_id = bm25_daat_current_doc_id(lhs);
    int32_t rhs_doc_id = bm25_daat_current_doc_id(rhs);

    if (lhs_doc_id < rhs_doc_id) {
        return 1;
    }
    if (lhs_doc_id > rhs_doc_id) {
        return 0;
    }
    if (lhs->slot->hash_value < rhs->slot->hash_value) {
        return 1;
    }
    if (lhs->slot->hash_value > rhs->slot->hash_value) {
        return 0;
    }
    return lhs->cursor < rhs->cursor;
}

static int32_t bm25_daat_collect_active_states(bm25_daat_term_state_t *states,
                                               int32_t state_count,
                                               bm25_daat_term_state_t **active_states)
{
    int32_t active_count = 0;
    int32_t i;

    for (i = 0; i < state_count; ++i) {
        bm25_daat_sync_filter_cursor(&states[i]);
        if (states[i].cursor >= states[i].slot->posting_count) {
            continue;
        }
        active_states[active_count] = &states[i];
        active_count += 1;
    }

    for (i = 1; i < active_count; ++i) {
        bm25_daat_term_state_t *current = active_states[i];
        int32_t insert_index = i;

        while (insert_index > 0 && bm25_daat_state_precedes(current, active_states[insert_index - 1])) {
            active_states[insert_index] = active_states[insert_index - 1];
            insert_index -= 1;
        }
        active_states[insert_index] = current;
    }

    return active_count;
}

static int32_t bm25_daat_find_pivot_index(bm25_daat_term_state_t **active_states,
                                          int32_t active_count,
                                          float threshold)
{
    float upper_bound = 0.0f;
    int32_t i;

    for (i = 0; i < active_count; ++i) {
        upper_bound += bm25_daat_current_block_upper_bound(active_states[i]);
        if (upper_bound > threshold) {
            return i;
        }
    }

    return -1;
}

static void bm25_daat_skip_to_next_block(bm25_daat_term_state_t *state, bm25_search_stats_t *stats)
{
    int32_t next_filter_cursor;
    int32_t skipped_postings;

    if (!state || !state->slot || state->cursor >= state->slot->posting_count) {
        return;
    }

    bm25_daat_sync_filter_cursor(state);
    if (state->filter_cursor >= state->slot->posting_filter_count) {
        state->cursor = state->slot->posting_count;
        return;
    }

    skipped_postings = state->slot->posting_filters[state->filter_cursor].end_offset - state->cursor;
    if (skipped_postings < 0) {
        skipped_postings = 0;
    }

    next_filter_cursor = state->filter_cursor + 1;
    if (stats) {
        stats->block_skip_count += 1;
        stats->skipped_postings_count += skipped_postings;
    }

    if (next_filter_cursor >= state->slot->posting_filter_count) {
        state->cursor = state->slot->posting_count;
        state->filter_cursor = state->slot->posting_filter_count;
        return;
    }

    state->filter_cursor = next_filter_cursor;
    state->cursor = state->slot->posting_filter_list[next_filter_cursor];
}

static void bm25_daat_advance_to_doc_with_stats(bm25_daat_term_state_t *state,
                                                int32_t target_doc,
                                                bm25_search_stats_t *stats)
{
    int32_t low;
    int32_t high;
    int32_t new_filter_cursor;

    if (!state || !state->slot || target_doc == INT_MAX) {
        return;
    }

    bm25_daat_sync_filter_cursor(state);
    if (state->cursor >= state->slot->posting_count || state->filter_cursor >= state->slot->posting_filter_count) {
        return;
    }

    if (state->slot->posting_filters[state->filter_cursor].last_doc_id < target_doc) {
        low = state->filter_cursor + 1;
        high = state->slot->posting_filter_count - 1;
        while (low <= high) {
            int32_t mid = low + (high - low) / 2;

            if (state->slot->posting_filters[mid].last_doc_id < target_doc) {
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }

        if (low >= state->slot->posting_filter_count) {
            if (stats) {
                stats->block_skip_count += state->slot->posting_filter_count - state->filter_cursor;
                stats->skipped_postings_count += state->slot->posting_count - state->cursor;
            }
            state->cursor = state->slot->posting_count;
            state->filter_cursor = state->slot->posting_filter_count;
            return;
        }

        new_filter_cursor = low;
        if (stats) {
            int32_t skipped_blocks = new_filter_cursor - state->filter_cursor;
            int32_t skipped_postings = state->slot->posting_filter_list[new_filter_cursor] - state->cursor;

            if (skipped_blocks > 0) {
                stats->block_skip_count += skipped_blocks;
            }
            if (skipped_postings > 0) {
                stats->skipped_postings_count += skipped_postings;
            }
        }
        state->filter_cursor = new_filter_cursor;
        state->cursor = state->slot->posting_filter_list[state->filter_cursor];
    }

    while (state->cursor < state->slot->posting_count &&
           state->slot->postings[state->cursor].doc_id < target_doc) {
        state->cursor += 1;
        bm25_daat_sync_filter_cursor(state);
    }
}

static int32_t bm25_daat_collect_prefix_count(bm25_daat_term_state_t **active_states,
                                              int32_t active_count,
                                              int32_t pivot_doc_id)
{
    int32_t prefix_count = 0;

    while (prefix_count < active_count && bm25_daat_current_doc_id(active_states[prefix_count]) <= pivot_doc_id) {
        prefix_count += 1;
    }
    return prefix_count;
}

static int bm25_daat_can_skip_pivot_block(bm25_daat_term_state_t **active_states,
                                          int32_t prefix_count,
                                          float threshold)
{
    float upper_bound = 0.0f;
    int32_t i;

    for (i = 0; i < prefix_count; ++i) {
        upper_bound += bm25_daat_current_block_upper_bound(active_states[i]);
    }
    return upper_bound <= threshold;
}

static int32_t bm25_daat_choose_multiblock_target(bm25_daat_term_state_t **active_states,
                                                  int32_t prefix_count,
                                                  int32_t current_active_count,
                                                  float threshold)
{
    int32_t candidate_docs[BM25_BMW_BLOCK_LOOKAHEAD * 32];
    int32_t candidate_count = 0;
    int32_t best_target = INT_MAX;
    int32_t max_states = prefix_count < current_active_count ? prefix_count : current_active_count;
    int32_t i;
    int32_t lookahead;

    if (threshold <= 0.0f || max_states <= 0) {
        return INT_MAX;
    }

    for (i = 0; i < max_states; ++i) {
        for (lookahead = 0; lookahead < BM25_BMW_BLOCK_LOOKAHEAD; ++lookahead) {
            int32_t candidate_doc = bm25_daat_candidate_block_target_doc(active_states[i], lookahead);
            int32_t duplicate = 0;
            int32_t j;

            if (candidate_doc == INT_MAX) {
                continue;
            }
            for (j = 0; j < candidate_count; ++j) {
                if (candidate_docs[j] == candidate_doc) {
                    duplicate = 1;
                    break;
                }
            }
            if (!duplicate && candidate_count < (int32_t)(sizeof(candidate_docs) / sizeof(candidate_docs[0]))) {
                candidate_docs[candidate_count] = candidate_doc;
                candidate_count += 1;
            }
        }
    }

    for (i = 1; i < candidate_count; ++i) {
        int32_t current_doc = candidate_docs[i];
        int32_t insert_index = i;

        while (insert_index > 0 && current_doc < candidate_docs[insert_index - 1]) {
            candidate_docs[insert_index] = candidate_docs[insert_index - 1];
            insert_index -= 1;
        }
        candidate_docs[insert_index] = current_doc;
    }

    for (i = 0; i < candidate_count; ++i) {
        float upper_bound = 0.0f;
        int32_t state_index;

        for (state_index = 0; state_index < current_active_count; ++state_index) {
            upper_bound += bm25_daat_range_block_upper_bound(active_states[state_index], candidate_docs[i]);
        }
        if (upper_bound <= threshold) {
            best_target = candidate_docs[i];
        } else {
            break;
        }
    }

    return best_target;
}

static int bm25_execute_daat(const bm25_t *index,
                             const bm25_term_freq_map_t *query_map,
                             int32_t k,
                             float *scores,
                             int32_t *doc_ids,
                             int32_t *hit_count)
{
    bm25_result_heap_t heap;
    bm25_search_stats_t *stats;
    bm25_daat_term_state_t *states;
    bm25_daat_term_state_t **active_states;
    int32_t active_term_count = 0;
    int32_t scored_hit_count = 0;
    int32_t queue_size;
    int32_t i;

    if (!index || !query_map || !hit_count) {
        return -1;
    }

    stats = (bm25_search_stats_t *)&index->last_search_stats;
    bm25_reset_search_stats(stats, BM25_SEARCH_ALGORITHM_DAAT);

    states = query_map->item_count > 0 ? (bm25_daat_term_state_t *)calloc((size_t)query_map->item_count, sizeof(bm25_daat_term_state_t)) : NULL;
    active_states = query_map->item_count > 0 ? (bm25_daat_term_state_t **)calloc((size_t)query_map->item_count, sizeof(bm25_daat_term_state_t *)) : NULL;
    if (query_map->item_count > 0 && (!states || !active_states)) {
        free(states);
        free(active_states);
        return -1;
    }

    for (i = 0; i < query_map->item_count; ++i) {
        const bm25_term_freq_entry_t *query_entry = query_map->items[i];
        int32_t term_slot_index = bm25_find_term_slot_index(index, query_entry->term, query_entry->hash_value);
        const bm25_term_slot_t *term_slot = bm25_get_term_slot(index, term_slot_index);

        if (!term_slot || term_slot->posting_count <= 0) {
            continue;
        }

        bm25_ensure_posting_filters(index, (bm25_term_slot_t *)term_slot);
        states[active_term_count].slot = term_slot;
        states[active_term_count].query_freq = query_entry->freq;
        states[active_term_count].cursor = 0;
        states[active_term_count].filter_cursor = 0;
        active_term_count += 1;
    }

    *hit_count = 0;
    if (active_term_count == 0) {
        free(states);
        free(active_states);
        return 0;
    }

    queue_size = bm25_effective_candidate_queue_size(index, k);
    if (bm25_result_heap_init(&heap, queue_size) != 0) {
        free(states);
        free(active_states);
        return -1;
    }

    while (true) {
        float threshold = bm25_result_heap_threshold(&heap);
        int32_t current_active_count;
        int32_t pivot_index;
        int32_t pivot_doc_id;
        int32_t target_doc;
        int32_t prefix_count;
        int32_t multiblock_target;

        current_active_count = bm25_daat_collect_active_states(states, active_term_count, active_states);
        if (current_active_count == 0) {
            break;
        }

        pivot_index = bm25_daat_find_pivot_index(active_states, current_active_count, threshold);
        if (pivot_index < 0) {
            bm25_daat_skip_to_next_block(active_states[0], stats);
            continue;
        }

        pivot_doc_id = bm25_daat_current_doc_id(active_states[pivot_index]);
        prefix_count = bm25_daat_collect_prefix_count(active_states, current_active_count, pivot_doc_id);
        multiblock_target = bm25_daat_choose_multiblock_target(active_states,
                                                               prefix_count,
                                                               current_active_count,
                                                               threshold);
        if (multiblock_target != INT_MAX) {
            for (i = 0; i < current_active_count; ++i) {
                if (bm25_daat_current_doc_id(active_states[i]) < multiblock_target) {
                    bm25_daat_advance_to_doc_with_stats(active_states[i], multiblock_target, stats);
                }
            }
            continue;
        }

        if (threshold > 0.0f && prefix_count > 0 &&
            bm25_daat_can_skip_pivot_block(active_states, prefix_count, threshold)) {
            bm25_daat_skip_to_next_block(active_states[prefix_count - 1], stats);
            continue;
        }

        for (i = 0; i < prefix_count; ++i) {
            if (bm25_daat_current_doc_id(active_states[i]) < pivot_doc_id) {
                bm25_daat_advance_to_doc_with_stats(active_states[i], pivot_doc_id, stats);
            }
        }

        current_active_count = bm25_daat_collect_active_states(states, active_term_count, active_states);
        if (current_active_count == 0) {
            break;
        }

        target_doc = bm25_daat_current_doc_id(active_states[0]);
        if (target_doc == INT_MAX) {
            break;
        }

        {
            float score = 0.0f;

            for (i = 0; i < current_active_count; ++i) {
                const bm25_posting_t *posting;

                bm25_daat_advance_to_doc_with_stats(active_states[i], target_doc, stats);
                if (active_states[i]->cursor >= active_states[i]->slot->posting_count) {
                    continue;
                }

                posting = &active_states[i]->slot->postings[active_states[i]->cursor];
                if (posting->doc_id != target_doc) {
                    continue;
                }

                score += bm25_calculate_term_score(index,
                                                   active_states[i]->slot->doc_freq,
                                                   posting->term_freq,
                                                   index->doc_lengths[posting->doc_id]) *
                         (float)active_states[i]->query_freq;
                active_states[i]->cursor += 1;
                bm25_daat_sync_filter_cursor(active_states[i]);
            }

            if (score > 0.0f) {
                scored_hit_count += 1;
                stats->scored_doc_count += 1;
                bm25_result_heap_consider(&heap, target_doc, score);
            }
        }
    }

    stats->candidate_count = scored_hit_count;
    stats->heap_compare_count = scored_hit_count;
    *hit_count = scored_hit_count;
    if (bm25_count_query_hits(index, query_map, hit_count) != 0) {
        bm25_result_heap_destroy(&heap);
        free(states);
        free(active_states);
        return -1;
    }
    if (bm25_fill_outputs_from_heap(&heap, k, scores, doc_ids) != 0) {
        bm25_result_heap_destroy(&heap);
        free(states);
        free(active_states);
        return -1;
    }
    bm25_result_heap_destroy(&heap);
    free(states);
    free(active_states);
    return 0;
}

int32_t bm25_index_search_with_count(const bm25_t *index,
                                     int32_t query_term_count,
                                     const char *const *query_terms,
                                     int32_t k,
                                     float *scores,
                                     int32_t *doc_ids,
                                     int32_t *hit_count)
{
    bm25_term_freq_map_t query_map;
    int32_t result;
    int32_t i;

    if (!index || k < 0 || (k > 0 && (!scores || !doc_ids)) || !hit_count ||
        query_term_count < 0 || (query_term_count > 0 && !query_terms)) {
        return -1;
    }

    for (i = 0; i < k; ++i) {
        scores[i] = 0.0f;
        doc_ids[i] = -1;
    }
    *hit_count = 0;

    if (k == 0 || index->doc_count == 0 || query_term_count == 0) {
        return 0;
    }

    if (bm25_collect_query_terms(index, query_term_count, query_terms, &query_map) != 0) {
        return -1;
    }
    if (query_map.item_count == 0) {
        bm25_term_freq_map_destroy(&query_map);
        return 0;
    }

    {
        int64_t start_us = bm25_get_time_us();

        if (bm25_pick_search_algorithm(index, &query_map, k) == BM25_SEARCH_ALGORITHM_DAAT) {
            result = bm25_execute_daat(index, &query_map, k, scores, doc_ids, hit_count);
        } else {
            result = bm25_execute_taat(index, &query_map, k, scores, doc_ids, hit_count);
        }

        ((bm25_search_stats_t *)&index->last_search_stats)->time_cost_us =
            bm25_get_time_us() - start_us;
    }

    bm25_term_freq_map_destroy(&query_map);
    return result;
}

int32_t bm25_index_search(const bm25_t *index,
                          int32_t query_term_count,
                          const char *const *query_terms,
                          int32_t k,
                          float *scores,
                          int32_t *doc_ids)
{
    int32_t hit_count;

    return bm25_index_search_with_count(index, query_term_count, query_terms, k, scores, doc_ids, &hit_count);
}

int32_t bm25_index_search_query_with_count(const bm25_t *index,
                                           const bm25_query_input_t *query,
                                           int32_t k,
                                           float *scores,
                                           int32_t *doc_ids,
                                           int32_t *hit_count)
{
    if (!index || !query) {
        return -1;
    }

    if (query->kind == BM25_INPUT_KIND_TOKENS) {
        return bm25_index_search_with_count(index,
                                            query->value.tokens.term_count,
                                            query->value.tokens.terms,
                                            k,
                                            scores,
                                            doc_ids,
                                            hit_count);
    }
    if (query->kind == BM25_INPUT_KIND_TEXT) {
        return bm25_index_search_text_with_count(index, query->value.text, k, scores, doc_ids, hit_count);
    }

    return -1;
}

int32_t bm25_index_search_query(const bm25_t *index,
                                const bm25_query_input_t *query,
                                int32_t k,
                                float *scores,
                                int32_t *doc_ids)
{
    int32_t hit_count;

    return bm25_index_search_query_with_count(index, query, k, scores, doc_ids, &hit_count);
}

int32_t bm25_index_search_text_with_count(const bm25_t *index,
                                          const char *query,
                                          int32_t k,
                                          float *scores,
                                          int32_t *doc_ids,
                                          int32_t *hit_count)
{
    token_t *tokens;
    const char **terms;
    int32_t token_count;
    int32_t result;
    int32_t i;

    if (!index || !query || !hit_count || k < 0 || (k > 0 && (!scores || !doc_ids))) {
        return -1;
    }

    tokens = NULL;
    token_count = 0;
    if (bm25_tokenize_text(index, query, &tokens, &token_count) != 0) {
        return -1;
    }

    if (token_count <= 0) {
        dict_free_tokens(tokens, token_count);
        return bm25_index_search_with_count(index, 0, NULL, k, scores, doc_ids, hit_count);
    }

    terms = (const char **)malloc((size_t)token_count * sizeof(const char *));
    if (!terms) {
        dict_free_tokens(tokens, token_count);
        return -1;
    }

    for (i = 0; i < token_count; ++i) {
        terms[i] = tokens[i].text;
    }

    result = bm25_index_search_with_count(index, token_count, terms, k, scores, doc_ids, hit_count);
    free(terms);
    dict_free_tokens(tokens, token_count);
    return result;
}

int32_t bm25_index_search_text(const bm25_t *index,
                               const char *query,
                               int32_t k,
                               float *scores,
                               int32_t *doc_ids)
{
    int32_t hit_count;

    return bm25_index_search_text_with_count(index, query, k, scores, doc_ids, &hit_count);
}
