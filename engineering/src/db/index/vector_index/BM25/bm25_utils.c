#include "bm25_private.h"

#define BM25_DEFAULT_CANDIDATE_QUEUE_SIZE 64
#define BM25_DEFAULT_POSTING_FILTER_WINDOW 16
#define BM25_DEFAULT_DAAT_THRESHOLD 64
#define BM25_INITIAL_TERM_BUCKET_COUNT 64

static int32_t bm25_next_capacity(int32_t current_capacity, int32_t target)
{
    int32_t new_capacity = current_capacity > 0 ? current_capacity : 8;

    while (new_capacity < target) {
        new_capacity *= 2;
    }
    return new_capacity;
}

static int bm25_term_freq_map_ensure_item_capacity(bm25_term_freq_map_t *map, int32_t target)
{
    bm25_term_freq_entry_t **new_items;
    int32_t new_capacity;

    if (!map || target < 0) {
        return -1;
    }
    if (target <= map->item_capacity) {
        return 0;
    }

    new_capacity = bm25_next_capacity(map->item_capacity, target);
    new_items = (bm25_term_freq_entry_t **)realloc(map->items, (size_t)new_capacity * sizeof(bm25_term_freq_entry_t *));
    if (!new_items) {
        return -1;
    }

    map->items = new_items;
    map->item_capacity = new_capacity;
    return 0;
}

static int bm25_rehash_term_buckets(bm25_t *index, int32_t new_bucket_count)
{
    bm25_term_bucket_entry_t **new_buckets;
    int32_t bucket_index;

    if (!index || new_bucket_count <= 0) {
        return -1;
    }

    new_buckets = (bm25_term_bucket_entry_t **)calloc((size_t)new_bucket_count, sizeof(bm25_term_bucket_entry_t *));
    if (!new_buckets) {
        return -1;
    }

    for (bucket_index = 0; bucket_index < index->term_hash_bucket_count; ++bucket_index) {
        bm25_term_bucket_entry_t *entry = index->term_hash_buckets[bucket_index];

        while (entry) {
            bm25_term_bucket_entry_t *next = entry->next;
            int32_t new_index = (int32_t)(entry->hash_value % (uint64_t)new_bucket_count);

            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            entry = next;
        }
    }

    free(index->term_hash_buckets);
    index->term_hash_buckets = new_buckets;
    index->term_hash_bucket_count = new_bucket_count;
    return 0;
}

static void bm25_release_term_slot(bm25_term_slot_t *slot)
{
    if (!slot) {
        return;
    }

    free(slot->term);
    free(slot->postings);
    free(slot->posting_filter_list);
    free(slot->posting_filters);
    memset(slot, 0, sizeof(*slot));
}

void bm25_set_default_params(bm25_params_t *params)
{
    if (!params) {
        return;
    }

    params->k1 = 1.2f;
    params->b = 0.75f;
    params->candidate_queue_size = BM25_DEFAULT_CANDIDATE_QUEUE_SIZE;
    params->posting_filter_window = BM25_DEFAULT_POSTING_FILTER_WINDOW;
    params->daat_threshold = BM25_DEFAULT_DAAT_THRESHOLD;
    params->search_algorithm = BM25_SEARCH_ALGORITHM_AUTO;
}

int bm25_params_are_valid(const bm25_params_t *params)
{
    if (!params) {
        return 0;
    }
    if (params->k1 < 0.0f) {
        return 0;
    }
    if (params->b < 0.0f || params->b > 1.0f) {
        return 0;
    }
    if (params->candidate_queue_size < 0 || params->posting_filter_window < 0 || params->daat_threshold < 0) {
        return 0;
    }
    if (params->search_algorithm < BM25_SEARCH_ALGORITHM_AUTO || params->search_algorithm > BM25_SEARCH_ALGORITHM_DAAT) {
        return 0;
    }
    return 1;
}

void bm25_normalize_params(bm25_params_t *params)
{
    if (!params) {
        return;
    }

    if (params->candidate_queue_size <= 0) {
        params->candidate_queue_size = BM25_DEFAULT_CANDIDATE_QUEUE_SIZE;
    }
    if (params->posting_filter_window <= 0) {
        params->posting_filter_window = BM25_DEFAULT_POSTING_FILTER_WINDOW;
    }
    if (params->daat_threshold <= 0) {
        params->daat_threshold = BM25_DEFAULT_DAAT_THRESHOLD;
    }
    if (params->search_algorithm < BM25_SEARCH_ALGORITHM_AUTO || params->search_algorithm > BM25_SEARCH_ALGORITHM_DAAT) {
        params->search_algorithm = BM25_SEARCH_ALGORITHM_AUTO;
    }
}

int bm25_ensure_default_tokenizer(bm25_t *index)
{
    dict_t *tokenizer;

    if (!index) {
        return -1;
    }
    if (index->tokenizer) {
        return 0;
    }

    tokenizer = dict_create_default();
    if (!tokenizer) {
        return -1;
    }

    index->tokenizer = tokenizer;
    index->owns_tokenizer = true;
    return 0;
}

char *bm25_strdup(const char *value)
{
    size_t length;
    char *copy;

    if (!value) {
        return NULL;
    }

    length = strlen(value);
    copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, value, length + 1);
    return copy;
}

uint64_t bm25_hash_term(const char *value)
{
    uint64_t hash = 1469598103934665603ULL;

    if (!value) {
        return 0;
    }

    while (*value) {
        hash ^= (unsigned char)*value;
        hash *= 1099511628211ULL;
        value += 1;
    }

    return hash;
}

int bm25_ensure_doc_capacity(bm25_t *index, int32_t target)
{
    int32_t *new_doc_lengths;
    bm25_doc_forward_list_t *new_doc_forwards;
    int32_t new_capacity;

    if (!index || target < 0) {
        return -1;
    }
    if (target <= index->doc_capacity) {
        return 0;
    }

    new_capacity = bm25_next_capacity(index->doc_capacity, target);

    new_doc_lengths = (int32_t *)realloc(index->doc_lengths, (size_t)new_capacity * sizeof(int32_t));
    if (!new_doc_lengths) {
        return -1;
    }
    memset(new_doc_lengths + index->doc_capacity,
           0,
           (size_t)(new_capacity - index->doc_capacity) * sizeof(int32_t));

    new_doc_forwards = (bm25_doc_forward_list_t *)realloc(index->doc_forwards,
                                                           (size_t)new_capacity * sizeof(bm25_doc_forward_list_t));
    if (!new_doc_forwards) {
        return -1;
    }
    memset(new_doc_forwards + index->doc_capacity,
           0,
           (size_t)(new_capacity - index->doc_capacity) * sizeof(bm25_doc_forward_list_t));

    index->doc_lengths = new_doc_lengths;
    index->doc_forwards = new_doc_forwards;
    index->doc_capacity = new_capacity;
    return 0;
}

int bm25_doc_forward_init(bm25_t *index, int32_t doc_id)
{
    bm25_doc_forward_list_t *forward;

    if (!index || doc_id < 0 || doc_id >= index->doc_capacity) {
        return -1;
    }

    forward = &index->doc_forwards[doc_id];
    if (forward->items && forward->capacity > 0) {
        forward->count = 0;
        return 0;
    }

    forward->items = (bm25_doc_forward_item_t *)calloc(4, sizeof(bm25_doc_forward_item_t));
    if (!forward->items) {
        return -1;
    }
    forward->count = 0;
    forward->capacity = 4;
    return 0;
}

int bm25_doc_forward_add_token(bm25_t *index, int32_t doc_id, int32_t term_slot_index, uint64_t hash_value)
{
    bm25_doc_forward_list_t *forward;
    bm25_doc_forward_item_t *new_items;
    int32_t new_capacity;

    if (!index || doc_id < 0 || doc_id >= index->doc_capacity) {
        return -1;
    }

    forward = &index->doc_forwards[doc_id];
    if (!forward->items) {
        if (bm25_doc_forward_init(index, doc_id) != 0) {
            return -1;
        }
    }

    if (forward->count >= forward->capacity) {
        new_capacity = forward->capacity * 2;
        new_items = (bm25_doc_forward_item_t *)realloc(forward->items,
                                                        (size_t)new_capacity * sizeof(bm25_doc_forward_item_t));
        if (!new_items) {
            return -1;
        }
        forward->items = new_items;
        forward->capacity = new_capacity;
    }

    forward->items[forward->count].term_slot_index = term_slot_index;
    forward->items[forward->count].token_hash = hash_value;
    forward->count += 1;
    return 0;
}

const bm25_doc_forward_list_t *bm25_doc_forward_get_tokens(const bm25_t *index, int32_t doc_id)
{
    if (!index || doc_id < 0 || doc_id >= index->doc_count) {
        return NULL;
    }
    if (!index->doc_forwards[doc_id].items || index->doc_forwards[doc_id].count == 0) {
        return NULL;
    }
    return &index->doc_forwards[doc_id];
}

void bm25_doc_forward_release(bm25_t *index, int32_t doc_id)
{
    if (!index || doc_id < 0 || doc_id >= index->doc_capacity) {
        return;
    }

    free(index->doc_forwards[doc_id].items);
    index->doc_forwards[doc_id].items = NULL;
    index->doc_forwards[doc_id].count = 0;
    index->doc_forwards[doc_id].capacity = 0;
}

void bm25_doc_forward_free_all(bm25_t *index)
{
    int32_t i;

    if (!index || !index->doc_forwards) {
        return;
    }

    for (i = 0; i < index->doc_capacity; ++i) {
        free(index->doc_forwards[i].items);
        index->doc_forwards[i].items = NULL;
        index->doc_forwards[i].count = 0;
        index->doc_forwards[i].capacity = 0;
    }
    free(index->doc_forwards);
    index->doc_forwards = NULL;
}

/* Posting 有序插入：二分定位 + memmove 后移。 */
int bm25_posting_insert_ordered(bm25_term_slot_t *slot, int32_t doc_id, int32_t term_freq)
{
    int32_t low;
    int32_t high;
    int32_t insert_index;
    int32_t move_count;

    if (!slot || doc_id < 0 || term_freq <= 0) {
        return -1;
    }

    if (bm25_reserve_postings(slot, slot->posting_count + 1) != 0) {
        return -1;
    }

    /* 二分定位插入位置 —— posting[] 按 doc_id 升序。 */
    low = 0;
    high = slot->posting_count;
    while (low < high) {
        int32_t mid = low + (high - low) / 2;

        if (slot->postings[mid].doc_id < doc_id) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    insert_index = low;

    if (insert_index < slot->posting_count && slot->postings[insert_index].doc_id == doc_id) {
        /* 已有该文档的 posting，累加词频。 */
        slot->postings[insert_index].term_freq += term_freq;
        return 0;
    }

    /* 后移插入位之后的元素。 */
    move_count = slot->posting_count - insert_index;
    if (move_count > 0) {
        memmove(&slot->postings[insert_index + 1],
                &slot->postings[insert_index],
                (size_t)move_count * sizeof(bm25_posting_t));
    }

    slot->postings[insert_index].doc_id = doc_id;
    slot->postings[insert_index].term_freq = term_freq;
    slot->posting_count += 1;
    return 0;
}

/* 从 posting[] 中移除指定下标的 posting。 */
int bm25_posting_remove_at(bm25_term_slot_t *slot, int32_t posting_index)
{
    int32_t move_count;

    if (!slot || posting_index < 0 || posting_index >= slot->posting_count) {
        return -1;
    }

    move_count = slot->posting_count - posting_index - 1;
    if (move_count > 0) {
        memmove(&slot->postings[posting_index],
                &slot->postings[posting_index + 1],
                (size_t)move_count * sizeof(bm25_posting_t));
    }
    slot->posting_count -= 1;
    return 0;
}

/* DocId 分配：优先从复用池获取。 */
int32_t bm25_allocate_doc_id(bm25_t *index)
{
    if (!index) {
        return -1;
    }

    if (index->free_doc_count > 0) {
        index->free_doc_count -= 1;
        return index->free_doc_ids[index->free_doc_count];
    }

    return index->doc_count;
}

/* DocId 回收到复用池。 */
void bm25_free_doc_id(bm25_t *index, int32_t doc_id)
{
    int32_t *new_pool;
    int32_t new_capacity;

    if (!index || doc_id < 0) {
        return;
    }

    if (index->free_doc_count >= index->free_doc_capacity) {
        new_capacity = index->free_doc_capacity > 0 ? index->free_doc_capacity * 2 : 16;
        new_pool = (int32_t *)realloc(index->free_doc_ids, (size_t)new_capacity * sizeof(int32_t));
        if (!new_pool) {
            return;
        }
        index->free_doc_ids = new_pool;
        index->free_doc_capacity = new_capacity;
    }

    index->free_doc_ids[index->free_doc_count] = doc_id;
    index->free_doc_count += 1;
}

int bm25_ensure_term_slot_capacity(bm25_t *index, int32_t target)
{
    bm25_term_slot_t *new_slots;
    int32_t new_capacity;

    if (!index || target < 0) {
        return -1;
    }
    if (target <= index->term_slot_capacity) {
        return 0;
    }

    new_capacity = bm25_next_capacity(index->term_slot_capacity, target);
    new_slots = (bm25_term_slot_t *)realloc(index->term_slots, (size_t)new_capacity * sizeof(bm25_term_slot_t));
    if (!new_slots) {
        return -1;
    }

    memset(new_slots + index->term_slot_capacity,
           0,
           (size_t)(new_capacity - index->term_slot_capacity) * sizeof(bm25_term_slot_t));
    index->term_slots = new_slots;
    index->term_slot_capacity = new_capacity;
    return 0;
}

int bm25_ensure_term_hash_table(bm25_t *index, int32_t target_term_count)
{
    int32_t new_bucket_count;

    if (!index || target_term_count < 0) {
        return -1;
    }
    if (index->term_hash_bucket_count == 0) {
        new_bucket_count = BM25_INITIAL_TERM_BUCKET_COUNT;
        while (new_bucket_count < target_term_count * 2) {
            new_bucket_count *= 2;
        }
        return bm25_rehash_term_buckets(index, new_bucket_count);
    }
    if (target_term_count * 4 < index->term_hash_bucket_count * 3) {
        return 0;
    }

    new_bucket_count = index->term_hash_bucket_count;
    while (new_bucket_count * 3 <= target_term_count * 4) {
        new_bucket_count *= 2;
    }

    if (new_bucket_count == index->term_hash_bucket_count) {
        return 0;
    }
    return bm25_rehash_term_buckets(index, new_bucket_count);
}

int32_t bm25_find_term_slot_index(const bm25_t *index, const char *term, uint64_t hash_value)
{
    bm25_term_bucket_entry_t *entry;
    int32_t bucket_index;

    if (!index || !term || !index->term_hash_buckets || index->term_hash_bucket_count <= 0) {
        return -1;
    }

    bucket_index = (int32_t)(hash_value % (uint64_t)index->term_hash_bucket_count);
    entry = index->term_hash_buckets[bucket_index];
    while (entry) {
        if (entry->hash_value == hash_value && strcmp(entry->term, term) == 0) {
            return entry->term_slot_index;
        }
        entry = entry->next;
    }

    return -1;
}

bm25_term_slot_t *bm25_get_term_slot(const bm25_t *index, int32_t term_slot_index)
{
    if (!index || term_slot_index < 0 || term_slot_index >= index->term_slot_count) {
        return NULL;
    }
    return &index->term_slots[term_slot_index];
}

int32_t bm25_insert_term_slot(bm25_t *index, const char *term, uint64_t hash_value)
{
    bm25_term_bucket_entry_t *bucket_entry;
    bm25_term_slot_t *slot;
    int32_t bucket_index;
    int32_t term_slot_index;

    if (!index || !term) {
        return -1;
    }
    if (bm25_ensure_term_slot_capacity(index, index->term_slot_count + 1) != 0 ||
        bm25_ensure_term_hash_table(index, index->term_slot_count + 1) != 0) {
        return -1;
    }

    term_slot_index = index->term_slot_count;
    slot = &index->term_slots[term_slot_index];
    memset(slot, 0, sizeof(*slot));
    slot->term = bm25_strdup(term);
    if (!slot->term) {
        return -1;
    }
    slot->hash_value = hash_value;

    bucket_entry = (bm25_term_bucket_entry_t *)calloc(1, sizeof(bm25_term_bucket_entry_t));
    if (!bucket_entry) {
        free(slot->term);
        memset(slot, 0, sizeof(*slot));
        return -1;
    }

    bucket_index = (int32_t)(hash_value % (uint64_t)index->term_hash_bucket_count);
    bucket_entry->term = slot->term;
    bucket_entry->hash_value = hash_value;
    bucket_entry->term_slot_index = term_slot_index;
    bucket_entry->next = index->term_hash_buckets[bucket_index];
    index->term_hash_buckets[bucket_index] = bucket_entry;
    index->term_slot_count += 1;
    return term_slot_index;
}

int bm25_remove_term_slot(bm25_t *index, int32_t term_slot_index)
{
    bm25_term_bucket_entry_t *current;
    bm25_term_bucket_entry_t *previous;
    bm25_term_slot_t *slot;
    int32_t bucket_index;

    if (!index || term_slot_index < 0 || term_slot_index != index->term_slot_count - 1) {
        return -1;
    }

    slot = &index->term_slots[term_slot_index];
    if (index->term_hash_bucket_count > 0 && slot->term) {
        bucket_index = (int32_t)(slot->hash_value % (uint64_t)index->term_hash_bucket_count);
        previous = NULL;
        current = index->term_hash_buckets[bucket_index];
        while (current) {
            if (current->term_slot_index == term_slot_index) {
                if (previous) {
                    previous->next = current->next;
                } else {
                    index->term_hash_buckets[bucket_index] = current->next;
                }
                free(current);
                break;
            }
            previous = current;
            current = current->next;
        }
    }

    bm25_release_term_slot(slot);
    index->term_slot_count -= 1;
    return 0;
}

int bm25_reserve_postings(bm25_term_slot_t *slot, int32_t target)
{
    bm25_posting_t *new_postings;
    int32_t new_capacity;

    if (!slot || target < 0) {
        return -1;
    }
    if (target <= slot->posting_capacity) {
        return 0;
    }

    new_capacity = bm25_next_capacity(slot->posting_capacity > 0 ? slot->posting_capacity : 4, target);
    new_postings = (bm25_posting_t *)realloc(slot->postings, (size_t)new_capacity * sizeof(bm25_posting_t));
    if (!new_postings) {
        return -1;
    }

    slot->postings = new_postings;
    slot->posting_capacity = new_capacity;
    return 0;
}

int bm25_reserve_posting_filters(bm25_term_slot_t *slot, int32_t target)
{
    bm25_posting_filter_t *new_filters;
    int32_t *new_filter_list;
    int32_t new_capacity;

    if (!slot || target < 0) {
        return -1;
    }
    if (target <= slot->posting_filter_capacity) {
        return 0;
    }

    new_capacity = bm25_next_capacity(slot->posting_filter_capacity > 0 ? slot->posting_filter_capacity : 4, target);
    new_filter_list = (int32_t *)malloc((size_t)new_capacity * sizeof(int32_t));
    if (!new_filter_list) {
        return -1;
    }

    new_filters = (bm25_posting_filter_t *)malloc((size_t)new_capacity * sizeof(bm25_posting_filter_t));
    if (!new_filters) {
        free(new_filter_list);
        return -1;
    }

    if (slot->posting_filter_count > 0) {
        memcpy(new_filter_list,
               slot->posting_filter_list,
               (size_t)slot->posting_filter_count * sizeof(int32_t));
        memcpy(new_filters,
               slot->posting_filters,
               (size_t)slot->posting_filter_count * sizeof(bm25_posting_filter_t));
    }

    free(slot->posting_filter_list);
    free(slot->posting_filters);

    slot->posting_filter_list = new_filter_list;
    slot->posting_filters = new_filters;
    slot->posting_filter_capacity = new_capacity;
    return 0;
}

void bm25_rebuild_posting_filters(const bm25_t *index, bm25_term_slot_t *slot)
{
    int32_t block_size;
    int32_t filter_count;
    int32_t filter_index;

    if (!index || !slot) {
        return;
    }

    block_size = index->params.posting_filter_window > 0 ? index->params.posting_filter_window : BM25_DEFAULT_POSTING_FILTER_WINDOW;
    filter_count = slot->posting_count > 0 ? (slot->posting_count + block_size - 1) / block_size : 0;
    slot->posting_filter_count = filter_count;

    for (filter_index = 0; filter_index < filter_count; ++filter_index) {
        bm25_posting_filter_t *filter = &slot->posting_filters[filter_index];
        int32_t start_offset = filter_index * block_size;
        int32_t end_offset = start_offset + block_size;
        int32_t posting_index;

        if (end_offset > slot->posting_count) {
            end_offset = slot->posting_count;
        }

        slot->posting_filter_list[filter_index] = start_offset;
        filter->start_offset = start_offset;
        filter->end_offset = end_offset;
        filter->last_doc_id = slot->postings[end_offset - 1].doc_id;
        filter->max_term_freq = 0;
        filter->max_score = 0.0f;

        for (posting_index = start_offset; posting_index < end_offset; ++posting_index) {
            const bm25_posting_t *posting = &slot->postings[posting_index];
            float score = bm25_calculate_term_score(index,
                                                    slot->doc_freq,
                                                    posting->term_freq,
                                                    index->doc_lengths[posting->doc_id]);

            if (posting->term_freq > filter->max_term_freq) {
                filter->max_term_freq = posting->term_freq;
            }
            if (score > filter->max_score) {
                filter->max_score = score;
            }
        }
    }
}

void bm25_free_terms(bm25_t *index)
{
    int32_t bucket_index;
    int32_t slot_index;

    if (!index) {
        return;
    }

    for (bucket_index = 0; bucket_index < index->term_hash_bucket_count; ++bucket_index) {
        bm25_term_bucket_entry_t *entry = index->term_hash_buckets[bucket_index];

        while (entry) {
            bm25_term_bucket_entry_t *next = entry->next;
            free(entry);
            entry = next;
        }
    }

    for (slot_index = 0; slot_index < index->term_slot_count; ++slot_index) {
        bm25_release_term_slot(&index->term_slots[slot_index]);
    }

    free(index->term_hash_buckets);
    free(index->term_slots);
    index->term_hash_buckets = NULL;
    index->term_hash_bucket_count = 0;
    index->term_slots = NULL;
    index->term_slot_count = 0;
    index->term_slot_capacity = 0;

    bm25_doc_forward_free_all(index);
    free(index->free_doc_ids);
    index->free_doc_ids = NULL;
    index->free_doc_count = 0;
    index->free_doc_capacity = 0;
}

int bm25_term_freq_map_init(bm25_term_freq_map_t *map, int32_t expected_terms)
{
    int32_t bucket_count;

    if (!map || expected_terms < 0) {
        return -1;
    }

    memset(map, 0, sizeof(*map));
    bucket_count = BM25_INITIAL_TERM_BUCKET_COUNT;
    while (bucket_count < expected_terms * 2) {
        bucket_count *= 2;
    }

    map->buckets = (bm25_term_freq_entry_t **)calloc((size_t)bucket_count, sizeof(bm25_term_freq_entry_t *));
    if (!map->buckets) {
        return -1;
    }

    map->bucket_count = bucket_count;
    return 0;
}

int bm25_term_freq_map_increment(bm25_term_freq_map_t *map, char *term, uint64_t hash_value)
{
    bm25_term_freq_entry_t *entry;
    int32_t bucket_index;

    if (!map || !term || map->bucket_count <= 0) {
        free(term);
        return -1;
    }

    bucket_index = (int32_t)(hash_value % (uint64_t)map->bucket_count);
    entry = map->buckets[bucket_index];
    while (entry) {
        if (entry->hash_value == hash_value && strcmp(entry->term, term) == 0) {
            entry->freq += 1;
            free(term);
            return 0;
        }
        entry = entry->next;
    }

    entry = (bm25_term_freq_entry_t *)calloc(1, sizeof(bm25_term_freq_entry_t));
    if (!entry) {
        free(term);
        return -1;
    }
    if (bm25_term_freq_map_ensure_item_capacity(map, map->item_count + 1) != 0) {
        free(term);
        free(entry);
        return -1;
    }

    entry->term = term;
    entry->hash_value = hash_value;
    entry->freq = 1;
    entry->next = map->buckets[bucket_index];
    map->buckets[bucket_index] = entry;
    map->items[map->item_count] = entry;
    map->item_count += 1;
    return 0;
}

void bm25_term_freq_map_destroy(bm25_term_freq_map_t *map)
{
    int32_t i;

    if (!map) {
        return;
    }

    for (i = 0; i < map->item_count; ++i) {
        free(map->items[i]->term);
        free(map->items[i]);
    }

    free(map->items);
    free(map->buckets);
    memset(map, 0, sizeof(*map));
}

int bm25_tokenize_text(const bm25_t *index, const char *text, token_t **tokens, int32_t *token_count)
{
    if (!index || !index->tokenizer || !text || !tokens || !token_count) {
        return -1;
    }

    return dict_cut(index->tokenizer, text, tokens, token_count);
}

float bm25_calculate_idf(const bm25_t *index, int32_t doc_freq)
{
    double numerator;
    double denominator;

    if (!index || index->doc_count <= 0 || doc_freq <= 0) {
        return 0.0f;
    }

    numerator = (double)index->doc_count - (double)doc_freq + 0.5;
    denominator = (double)doc_freq + 0.5;
    return (float)log(1.0 + numerator / denominator);
}

float bm25_calculate_term_score(const bm25_t *index, int32_t doc_freq, int32_t term_freq, int32_t doc_length)
{
    double average_doc_length;
    double denominator;
    double tf_norm;
    double idf;

    if (!index || index->doc_count <= 0 || term_freq <= 0 || doc_length < 0) {
        return 0.0f;
    }

    average_doc_length = (double)index->total_terms / (double)index->doc_count;
    if (average_doc_length <= 0.0) {
        average_doc_length = 1.0;
    }

    denominator = (double)term_freq +
                  (double)index->params.k1 *
                      (1.0 - (double)index->params.b +
                       (double)index->params.b * ((double)doc_length / average_doc_length));
    if (denominator <= 0.0) {
        return 0.0f;
    }

    tf_norm = ((double)term_freq * ((double)index->params.k1 + 1.0)) / denominator;
    idf = (double)bm25_calculate_idf(index, doc_freq);
    return (float)(idf * tf_norm);
}

int bm25_compare_scored_docs_desc(const void *lhs, const void *rhs)
{
    const bm25_scored_doc_t *left = (const bm25_scored_doc_t *)lhs;
    const bm25_scored_doc_t *right = (const bm25_scored_doc_t *)rhs;

    if (left->score > right->score) {
        return -1;
    }
    if (left->score < right->score) {
        return 1;
    }
    if (left->doc_id < right->doc_id) {
        return -1;
    }
    if (left->doc_id > right->doc_id) {
        return 1;
    }
    return 0;
}

/* 确保 posting filter 有效，dirty 时重建。 */
void bm25_ensure_posting_filters(const bm25_t *index, bm25_term_slot_t *slot)
{
    if (!index || !slot) {
        return;
    }
    if (slot->posting_filter_count < 0) {
        bm25_reserve_posting_filters(slot,
            slot->posting_count > 0 ? (slot->posting_count + index->params.posting_filter_window - 1) / index->params.posting_filter_window : 0);
        bm25_rebuild_posting_filters(index, slot);
    }
}

void bm25_reset_search_stats(bm25_search_stats_t *stats, bm25_search_algorithm_t algorithm)
{
    if (!stats) {
        return;
    }

    memset(stats, 0, sizeof(*stats));
    stats->algorithm = algorithm;
}