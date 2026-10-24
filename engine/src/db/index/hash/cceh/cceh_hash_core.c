/*
 * cceh_hash_core.c
 *
 * Core helpers and lifecycle for the in-memory CCEH index.
 */

#include "cceh_hash_private.h"

#if defined(_WIN32)
#include <windows.h>
#include <malloc.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#else
#include <pthread.h>
#endif

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <cpuid.h>
#endif

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <immintrin.h>
#endif

typedef enum cceh_persist_backend_kind {
    CCEH_PERSIST_BACKEND_FENCE = 0,
    CCEH_PERSIST_BACKEND_CLFLUSH = 1,
    CCEH_PERSIST_BACKEND_CLFLUSHOPT = 2,
    CCEH_PERSIST_BACKEND_CLWB = 3
} cceh_persist_backend_kind_t;

typedef struct cceh_tls_epoch_entry {
    cceh_index_t                 *index;
    cceh_thread_epoch_t          *epoch_record;
    struct cceh_tls_epoch_entry  *next;
} cceh_tls_epoch_entry_t;

static _Thread_local cceh_tls_epoch_entry_t *g_cceh_tls_epochs = NULL;
static atomic_uint g_cceh_tls_backend_state = ATOMIC_VAR_INIT(0u);
static atomic_flag g_cceh_epoch_registry_latch = ATOMIC_FLAG_INIT;

#if defined(_WIN32)
static DWORD g_cceh_tls_fls_index = FLS_OUT_OF_INDEXES;
#else
static pthread_key_t g_cceh_tls_key;
#endif

static void _cceh_tls_epoch_cleanup_list(cceh_tls_epoch_entry_t *head);

static void _cceh_epoch_registry_lock(void)
{
    while (atomic_flag_test_and_set_explicit(&g_cceh_epoch_registry_latch, memory_order_acquire)) {
    }
}

static void _cceh_epoch_registry_unlock(void)
{
    atomic_flag_clear_explicit(&g_cceh_epoch_registry_latch, memory_order_release);
}

#if defined(_WIN32)
static VOID CALLBACK _cceh_tls_epoch_cleanup_callback(PVOID value)
{
    _cceh_tls_epoch_cleanup_list((cceh_tls_epoch_entry_t *)value);
}
#else
static void _cceh_tls_epoch_cleanup_callback(void *value)
{
    _cceh_tls_epoch_cleanup_list((cceh_tls_epoch_entry_t *)value);
}
#endif

static size_t _cceh_round_up(size_t size, size_t alignment)
{
    return (size + alignment - 1u) & ~(alignment - 1u);
}

static uint32_t _cceh_depth_mask(uint32_t depth)
{
    if (depth >= 32u) {
        return UINT32_MAX;
    }
    return (1u << depth) - 1u;
}

static cceh_persist_backend_kind_t _cceh_persist_backend_kind(void)
{
    static atomic_int cached_backend = ATOMIC_VAR_INIT(-1);
    int backend = atomic_load_explicit(&cached_backend, memory_order_acquire);

    if (backend >= 0) {
        return (cceh_persist_backend_kind_t)backend;
    }

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
    {
        bool has_clflush = false;
        bool has_clflushopt = false;
        bool has_clwb = false;

#if defined(_MSC_VER)
        int cpu_info[4] = {0, 0, 0, 0};

        __cpuid(cpu_info, 1);
        has_clflush = (cpu_info[3] & (1 << 19)) != 0;
        __cpuidex(cpu_info, 7, 0);
        has_clflushopt = (cpu_info[1] & (1 << 23)) != 0;
        has_clwb = (cpu_info[1] & (1 << 24)) != 0;
#elif defined(__GNUC__)
        unsigned int eax = 0;
        unsigned int ebx = 0;
        unsigned int ecx = 0;
        unsigned int edx = 0;

        if (__get_cpuid_max(0, NULL) >= 1u && __get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            has_clflush = (edx & (1u << 19)) != 0u;
        }
        if (__get_cpuid_max(0, NULL) >= 7u && __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
            has_clflushopt = (ebx & (1u << 23)) != 0u;
            has_clwb = (ebx & (1u << 24)) != 0u;
        }
#endif

        if (has_clwb) {
            backend = CCEH_PERSIST_BACKEND_CLWB;
        } else if (has_clflushopt) {
            backend = CCEH_PERSIST_BACKEND_CLFLUSHOPT;
        } else if (has_clflush) {
            backend = CCEH_PERSIST_BACKEND_CLFLUSH;
        } else {
            backend = CCEH_PERSIST_BACKEND_FENCE;
        }
    }
#else
    backend = CCEH_PERSIST_BACKEND_FENCE;
#endif

    atomic_store_explicit(&cached_backend, backend, memory_order_release);
    return (cceh_persist_backend_kind_t)backend;
}

static bool _cceh_tls_backend_ensure(void)
{
    unsigned int state = atomic_load_explicit(&g_cceh_tls_backend_state, memory_order_acquire);

    if (state == 2u) {
        return true;
    }
    if (state == 3u) {
        return false;
    }

    state = 0u;
    if (atomic_compare_exchange_strong_explicit(&g_cceh_tls_backend_state,
                                                &state,
                                                1u,
                                                memory_order_acq_rel,
                                                memory_order_acquire)) {
        bool ok;

#if defined(_WIN32)
        g_cceh_tls_fls_index = FlsAlloc(_cceh_tls_epoch_cleanup_callback);
        ok = g_cceh_tls_fls_index != FLS_OUT_OF_INDEXES;
#else
        ok = pthread_key_create(&g_cceh_tls_key, _cceh_tls_epoch_cleanup_callback) == 0;
#endif

        atomic_store_explicit(&g_cceh_tls_backend_state,
                              ok ? 2u : 3u,
                              memory_order_release);
        return ok;
    }

    do {
        state = atomic_load_explicit(&g_cceh_tls_backend_state, memory_order_acquire);
    } while (state == 1u);

    return state == 2u;
}

static void _cceh_tls_epoch_publish(cceh_tls_epoch_entry_t *head)
{
    g_cceh_tls_epochs = head;
    if (!_cceh_tls_backend_ensure()) {
        return;
    }

#if defined(_WIN32)
    (void)FlsSetValue(g_cceh_tls_fls_index, head);
#else
    (void)pthread_setspecific(g_cceh_tls_key, head);
#endif
}

static void _cceh_thread_epoch_unregister(cceh_thread_epoch_t *epoch_record)
{
    cceh_index_t *index;

    if (!epoch_record) {
        return;
    }

    atomic_store_explicit(&epoch_record->active, false, memory_order_release);
    _cceh_epoch_registry_lock();
    index = (cceh_index_t *)(uintptr_t)atomic_exchange_explicit(&epoch_record->owner_index,
                                                                (uintptr_t)NULL,
                                                                memory_order_acq_rel);
    if (index) {
        cceh_thread_epoch_t **cursor;

        _cceh_directory_lock(index);
        cursor = &index->thread_epochs;
        while (*cursor) {
            if (*cursor == epoch_record) {
                *cursor = epoch_record->next;
                break;
            }
            cursor = &(*cursor)->next;
        }
        _cceh_reclaim_retired(index);
        _cceh_directory_unlock(index);
    }
    _cceh_epoch_registry_unlock();

    free(epoch_record);
}

static void _cceh_tls_epoch_cleanup_list(cceh_tls_epoch_entry_t *head)
{
    cceh_tls_epoch_entry_t *entry = head;

    while (entry) {
        cceh_tls_epoch_entry_t *next = entry->next;

        _cceh_thread_epoch_unregister(entry->epoch_record);
        free(entry);
        entry = next;
    }

    g_cceh_tls_epochs = NULL;
    if (atomic_load_explicit(&g_cceh_tls_backend_state, memory_order_acquire) == 2u) {
#if defined(_WIN32)
        (void)FlsSetValue(g_cceh_tls_fls_index, NULL);
#else
        (void)pthread_setspecific(g_cceh_tls_key, NULL);
#endif
    }
}

static cceh_tls_epoch_entry_t *_cceh_tls_epoch_find(cceh_index_t *index)
{
    cceh_tls_epoch_entry_t *entry = g_cceh_tls_epochs;

    while (entry) {
        if (entry->index == index) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static cceh_thread_epoch_t *_cceh_thread_epoch_register(cceh_index_t *index)
{
    cceh_thread_epoch_t *epoch_record;
    cceh_tls_epoch_entry_t *tls_entry;

    epoch_record = (cceh_thread_epoch_t *)calloc(1, sizeof(cceh_thread_epoch_t));
    if (!epoch_record) {
        return NULL;
    }
    tls_entry = (cceh_tls_epoch_entry_t *)calloc(1, sizeof(cceh_tls_epoch_entry_t));
    if (!tls_entry) {
        free(epoch_record);
        return NULL;
    }

    atomic_init(&epoch_record->thread_id, (uintptr_t)epoch_record);
    atomic_init(&epoch_record->owner_index, (uintptr_t)index);
    atomic_init(&epoch_record->epoch, 0u);
    atomic_init(&epoch_record->active, false);

    _cceh_epoch_registry_lock();
    _cceh_directory_lock(index);
    epoch_record->next = index->thread_epochs;
    index->thread_epochs = epoch_record;
    _cceh_directory_unlock(index);
    _cceh_epoch_registry_unlock();

    tls_entry->index = index;
    tls_entry->epoch_record = epoch_record;
    tls_entry->next = g_cceh_tls_epochs;
    _cceh_tls_epoch_publish(tls_entry);
    return epoch_record;
}

static cceh_record_t *_cceh_slot_record(const cceh_slot_t *slot)
{
    return (cceh_record_t *)(uintptr_t)atomic_load_explicit(&slot->record_ptr, memory_order_acquire);
}

uint32_t _cceh_hash_func(const void *key, uint32_t keylen)
{
    const uint8_t *data = (const uint8_t *)key;
    uint32_t hash = 2166136261u;
    uint32_t i;

    for (i = 0; i < keylen; i++) {
        hash ^= (uint32_t)data[i];
        hash *= 16777619u;
    }
    return hash;
}

uint8_t _cceh_fingerprint(uint32_t hashval)
{
    uint8_t fingerprint;

    fingerprint = (uint8_t)(hashval ^ (hashval >> 8) ^ (hashval >> 16) ^ (hashval >> 24));
    return fingerprint == 0u ? 1u : fingerprint;
}

uint32_t _cceh_directory_index(const cceh_directory_t *directory, uint32_t hashval)
{
    return hashval & _cceh_depth_mask(directory->global_depth);
}

void *_cceh_aligned_alloc(size_t alignment, size_t size)
{
    size_t rounded_size = _cceh_round_up(size, alignment);

#if defined(_WIN32)
    return _aligned_malloc(rounded_size, alignment);
#else
    void *ptr = NULL;

    if (posix_memalign(&ptr, alignment, rounded_size) != 0) {
        return NULL;
    }
    return ptr;
#endif
}

void _cceh_aligned_free(void *ptr)
{
    if (!ptr) {
        return;
    }

#if defined(_WIN32)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

void _cceh_flush_range(const void *addr, size_t len)
{
    cceh_persist_backend_kind_t backend;
    uintptr_t cursor;
    uintptr_t end;

    if (!addr || len == 0u) {
        return;
    }

    backend = _cceh_persist_backend_kind();
    if (backend == CCEH_PERSIST_BACKEND_FENCE) {
        atomic_thread_fence(memory_order_release);
        return;
    }

    cursor = ((uintptr_t)addr) & ~((uintptr_t)CCEH_CACHELINE_SIZE - 1u);
    end = (uintptr_t)addr + len;

    while (cursor < end) {
#if defined(__CLWB__)
        if (backend == CCEH_PERSIST_BACKEND_CLWB) {
            _mm_clwb((void *)cursor);
        } else
#endif
#if defined(__CLFLUSHOPT__)
        if (backend == CCEH_PERSIST_BACKEND_CLFLUSHOPT) {
            _mm_clflushopt((void *)cursor);
        } else
#endif
        {
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
            _mm_clflush((const void *)cursor);
#else
            atomic_signal_fence(memory_order_seq_cst);
#endif
        }
        cursor += CCEH_CACHELINE_SIZE;
    }
}

void _cceh_drain_writes(void)
{
    switch (_cceh_persist_backend_kind()) {
        case CCEH_PERSIST_BACKEND_FENCE:
            atomic_thread_fence(memory_order_seq_cst);
            break;
        default:
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
            _mm_sfence();
#else
            atomic_thread_fence(memory_order_seq_cst);
#endif
            break;
    }
}

void _cceh_persist_range(const void *addr, size_t len)
{
    _cceh_flush_range(addr, len);
    _cceh_drain_writes();
}

void _cceh_publish_changes(cceh_index_t *index,
                           const void *addr,
                           size_t len,
                           cceh_segment_t *segment)
{
    uint32_t epoch;

    if (addr && len > 0u) {
        _cceh_persist_range(addr, len);
    }

    if (!index) {
        return;
    }

    epoch = atomic_fetch_add_explicit(&index->persist_epoch, 1u, memory_order_acq_rel) + 1u;
    if (segment) {
        segment->persist_epoch = epoch;
        _cceh_persist_range(segment, sizeof(*segment));
    }
}

void _cceh_directory_lock(cceh_index_t *index)
{
    if (!index) {
        return;
    }

    while (atomic_flag_test_and_set_explicit(&index->directory_latch, memory_order_acquire)) {
    }
}

void _cceh_directory_unlock(cceh_index_t *index)
{
    if (!index) {
        return;
    }

    atomic_flag_clear_explicit(&index->directory_latch, memory_order_release);
}

void _cceh_segment_lock(cceh_segment_t *segment)
{
    if (!segment) {
        return;
    }

    while (atomic_flag_test_and_set_explicit(&segment->latch, memory_order_acquire)) {
    }
}

void _cceh_segment_unlock(cceh_segment_t *segment)
{
    if (!segment) {
        return;
    }

    atomic_flag_clear_explicit(&segment->latch, memory_order_release);
}

void _cceh_segment_pin(cceh_segment_t *segment)
{
    if (!segment) {
        return;
    }

    atomic_fetch_add_explicit(&segment->pin_count, 1u, memory_order_acq_rel);
}

void _cceh_segment_unpin(cceh_segment_t *segment)
{
    if (!segment) {
        return;
    }

    atomic_fetch_sub_explicit(&segment->pin_count, 1u, memory_order_acq_rel);
}

void _cceh_reader_enter(cceh_index_t *index)
{
    cceh_tls_epoch_entry_t *entry;
    cceh_thread_epoch_t *epoch_record;

    if (!index) {
        return;
    }

    entry = _cceh_tls_epoch_find(index);
    epoch_record = entry ? entry->epoch_record : _cceh_thread_epoch_register(index);
    if (!epoch_record) {
        return;
    }

    atomic_store_explicit(&epoch_record->epoch,
                          atomic_load_explicit(&index->global_epoch, memory_order_acquire),
                          memory_order_release);
    atomic_store_explicit(&epoch_record->active, true, memory_order_release);
}

void _cceh_reader_exit(cceh_index_t *index)
{
    cceh_tls_epoch_entry_t *entry;

    if (!index) {
        return;
    }

    entry = _cceh_tls_epoch_find(index);
    if (!entry || !entry->epoch_record) {
        return;
    }

    atomic_store_explicit(&entry->epoch_record->active, false, memory_order_release);
}
uint32_t _cceh_advance_global_epoch(cceh_index_t *index)
{
    return atomic_fetch_add_explicit(&index->global_epoch, 1u, memory_order_acq_rel) + 1u;
}

uint32_t _cceh_min_active_epoch(const cceh_index_t *index)
{
    cceh_thread_epoch_t *node;
    uint32_t min_epoch = UINT32_MAX;
    bool found_active = false;

    if (!index) {
        return UINT32_MAX;
    }

    node = index->thread_epochs;
    while (node) {
        if (atomic_load_explicit(&node->active, memory_order_acquire)) {
            uint32_t epoch = atomic_load_explicit(&node->epoch, memory_order_acquire);

            if (!found_active || epoch < min_epoch) {
                min_epoch = epoch;
            }
            found_active = true;
        }
        node = node->next;
    }

    return found_active ? min_epoch : UINT32_MAX;
}


void _cceh_segment_begin_write(cceh_segment_t *segment)
{
    atomic_fetch_add_explicit(&segment->version, 1u, memory_order_acq_rel);
}

void _cceh_segment_end_write(cceh_segment_t *segment)
{
    atomic_fetch_add_explicit(&segment->version, 1u, memory_order_release);
}

cceh_directory_t *_cceh_directory_create(uint32_t global_depth,
                                         uint32_t size,
                                         uint32_t version)
{
    cceh_directory_t *directory;

    directory = (cceh_directory_t *)_cceh_aligned_alloc(CCEH_CACHELINE_SIZE, sizeof(cceh_directory_t));
    if (!directory) {
        return NULL;
    }
    memset(directory, 0, sizeof(*directory));

    directory->segments = (cceh_segment_t **)_cceh_aligned_alloc(CCEH_CACHELINE_SIZE,
                                                                 (size_t)size * sizeof(cceh_segment_t *));
    if (!directory->segments) {
        _cceh_aligned_free(directory);
        return NULL;
    }
    memset(directory->segments, 0, (size_t)size * sizeof(cceh_segment_t *));

    directory->version = version;
    directory->global_depth = global_depth;
    directory->size = size;
    return directory;
}

void _cceh_directory_drop(cceh_directory_t *directory)
{
    if (!directory) {
        return;
    }

    _cceh_aligned_free(directory->segments);
    _cceh_aligned_free(directory);
}

int _cceh_publish_directory(cceh_index_t *index,
                            cceh_directory_t *old_directory,
                            cceh_directory_t *new_directory)
{
    if (!index || !new_directory) {
        return -1;
    }

    _cceh_publish_changes(index,
                          new_directory->segments,
                          (size_t)new_directory->size * sizeof(cceh_segment_t *),
                          NULL);
    _cceh_persist_range(new_directory, sizeof(*new_directory));
    atomic_store_explicit(&index->directory_root, new_directory, memory_order_release);

    if (old_directory) {
        _cceh_retire_directory(index, old_directory);
    }
    return 0;
}

void _cceh_retire_directory(cceh_index_t *index, cceh_directory_t *directory)
{
    cceh_retired_directory_t *node;

    if (!index || !directory) {
        return;
    }

    node = (cceh_retired_directory_t *)calloc(1, sizeof(cceh_retired_directory_t));
    if (!node) {
        return;
    }

    node->directory = directory;
    node->retire_epoch = _cceh_advance_global_epoch(index);
    node->next = index->retired_directories;
    index->retired_directories = node;
}

void _cceh_retire_segment(cceh_index_t *index,
                          cceh_segment_t *segment,
                          bool drop_records)
{
    cceh_retired_segment_t *node;

    if (!index || !segment) {
        return;
    }

    node = (cceh_retired_segment_t *)calloc(1, sizeof(cceh_retired_segment_t));
    if (!node) {
        return;
    }

    atomic_store_explicit(&segment->retired, true, memory_order_release);
    _cceh_persist_range(segment, sizeof(*segment));
    node->segment = segment;
    node->retire_epoch = _cceh_advance_global_epoch(index);
    node->drop_records = drop_records;
    node->next = index->retired_segments;
    index->retired_segments = node;
}

void _cceh_retire_record(cceh_index_t *index, cceh_record_t *record)
{
    cceh_retired_record_t *node;

    if (!index || !record) {
        return;
    }

    node = (cceh_retired_record_t *)calloc(1, sizeof(cceh_retired_record_t));
    if (!node) {
        return;
    }

    node->record = record;
    node->retire_epoch = _cceh_advance_global_epoch(index);
    node->next = index->retired_records;
    index->retired_records = node;
}

void _cceh_reclaim_retired(cceh_index_t *index)
{
    cceh_retired_directory_t *directory_node;
    cceh_retired_segment_t *segment_node;
    cceh_retired_record_t *record_node;
    uint32_t min_active_epoch;

    if (!index) {
        return;
    }

    min_active_epoch = _cceh_min_active_epoch(index);

    directory_node = index->retired_directories;
    index->retired_directories = NULL;
    while (directory_node) {
        cceh_retired_directory_t *next = directory_node->next;

        if (directory_node->retire_epoch < min_active_epoch) {
            _cceh_directory_drop(directory_node->directory);
            free(directory_node);
        } else {
            directory_node->next = index->retired_directories;
            index->retired_directories = directory_node;
        }
        directory_node = next;
    }

    segment_node = index->retired_segments;
    index->retired_segments = NULL;
    while (segment_node) {
        cceh_retired_segment_t *next = segment_node->next;

        if (segment_node->retire_epoch < min_active_epoch &&
            atomic_load_explicit(&segment_node->segment->pin_count, memory_order_acquire) == 0u) {
            _cceh_segment_drop(segment_node->segment, segment_node->drop_records);
            free(segment_node);
        } else {
            segment_node->next = index->retired_segments;
            index->retired_segments = segment_node;
        }
        segment_node = next;
    }

    record_node = index->retired_records;
    index->retired_records = NULL;
    while (record_node) {
        cceh_retired_record_t *next = record_node->next;

        if (record_node->retire_epoch < min_active_epoch) {
            _cceh_record_drop(record_node->record);
            free(record_node);
        } else {
            record_node->next = index->retired_records;
            index->retired_records = record_node;
        }
        record_node = next;
    }
}

int _cceh_segment_clone_with_change(cceh_segment_t *source,
                                    uint32_t hashval,
                                    uint8_t fingerprint,
                                    const void *key,
                                    uint32_t keylen,
                                    const void *value,
                                    uint32_t valuelen,
                                    bool remove_existing,
                                    bool allow_insert_if_missing,
                                    bool *changed_out,
                                    cceh_segment_t **replacement_out,
                                    cceh_record_t **replaced_record_out)
{
    cceh_segment_t *replacement;
    cceh_record_t *new_record;
    cceh_record_t *replaced_record;
    uint32_t i;
    bool found = false;

    if (!source || !replacement_out) {
        return -1;
    }

    replacement = _cceh_segment_create(source->local_depth, source->capacity);
    if (!replacement) {
        return -1;
    }

    new_record = NULL;
    replaced_record = NULL;

    for (i = 0; i < source->capacity; i++) {
        cceh_record_t *record;

        if (atomic_load_explicit(&source->slots[i].state, memory_order_acquire) != CCEH_SLOT_LIVE) {
            continue;
        }

        record = _cceh_slot_record(&source->slots[i]);
        if (!found &&
            atomic_load_explicit(&source->slots[i].fingerprint, memory_order_acquire) == fingerprint &&
            record &&
            record->hashval == hashval &&
            record->keylen == keylen &&
            memcmp(record->key, key, keylen) == 0) {
            found = true;
            replaced_record = record;

            if (!remove_existing) {
                new_record = _cceh_record_create(hashval, key, keylen, value, valuelen);
                if (!new_record || _cceh_segment_store_record(replacement, new_record, NULL) != 0) {
                    _cceh_record_drop(new_record);
                    _cceh_segment_drop(replacement, true);
                    return -1;
                }
            }
            continue;
        }

        if (_cceh_segment_store_record(replacement, record, NULL) != 0) {
            if (new_record) {
                _cceh_record_drop(new_record);
            }
            _cceh_segment_drop(replacement, false);
            return -1;
        }
    }

    if (!found) {
        if (!allow_insert_if_missing) {
            _cceh_segment_drop(replacement, false);
            return 1;
        }

        new_record = _cceh_record_create(hashval, key, keylen, value, valuelen);
        if (!new_record || _cceh_segment_store_record(replacement, new_record, NULL) != 0) {
            _cceh_record_drop(new_record);
            _cceh_segment_drop(replacement, false);
            return 1;
        }
        found = true;
    }

    if (changed_out) {
        *changed_out = found;
    }
    if (replaced_record_out) {
        *replaced_record_out = replaced_record;
    }
    *replacement_out = replacement;
    return 0;
}

cceh_record_t *_cceh_record_create(uint32_t hashval,
                                   const void *key,
                                   uint32_t keylen,
                                   const void *value,
                                   uint32_t valuelen)
{
    cceh_record_t *record;

    record = (cceh_record_t *)calloc(1, sizeof(cceh_record_t));
    if (!record) {
        return NULL;
    }

    record->hashval = hashval;
    record->keylen = keylen;
    record->valuelen = valuelen;

    record->key = malloc(keylen);
    if (!record->key) {
        free(record);
        return NULL;
    }
    memcpy(record->key, key, keylen);

    if (value && valuelen > 0u) {
        record->value = malloc(valuelen);
        if (!record->value) {
            free(record->key);
            free(record);
            return NULL;
        }
        memcpy(record->value, value, valuelen);
    }

    return record;
}

void _cceh_record_drop(cceh_record_t *record)
{
    if (!record) {
        return;
    }

    free(record->key);
    free(record->value);
    free(record);
}

cceh_segment_t *_cceh_segment_create(uint32_t local_depth, uint32_t capacity)
{
    cceh_segment_t *segment;
    uint32_t i;

    segment = (cceh_segment_t *)_cceh_aligned_alloc(CCEH_CACHELINE_SIZE, sizeof(cceh_segment_t));
    if (!segment) {
        return NULL;
    }
    memset(segment, 0, sizeof(*segment));

    segment->slots = (cceh_slot_t *)_cceh_aligned_alloc(CCEH_CACHELINE_SIZE,
                                                        (size_t)capacity * sizeof(cceh_slot_t));
    if (!segment->slots) {
        _cceh_aligned_free(segment);
        return NULL;
    }
    memset(segment->slots, 0, (size_t)capacity * sizeof(cceh_slot_t));

    atomic_flag_clear(&segment->latch);
    atomic_init(&segment->pin_count, 0u);
    atomic_init(&segment->version, 0u);
    atomic_init(&segment->retired, false);
    segment->local_depth = local_depth;
    segment->capacity = capacity;

    for (i = 0; i < capacity; i++) {
        atomic_init(&segment->slots[i].fingerprint, 0u);
        atomic_init(&segment->slots[i].state, CCEH_SLOT_EMPTY);
        atomic_init(&segment->slots[i].record_ptr, (uintptr_t)NULL);
    }

    _cceh_persist_range(segment->slots, (size_t)capacity * sizeof(cceh_slot_t));
    _cceh_persist_range(segment, sizeof(*segment));
    return segment;
}

void _cceh_segment_drop(cceh_segment_t *segment, bool drop_records)
{
    uint32_t i;

    if (!segment) {
        return;
    }

    if (drop_records) {
        for (i = 0; i < segment->capacity; i++) {
            if (atomic_load_explicit(&segment->slots[i].state, memory_order_acquire) == CCEH_SLOT_LIVE) {
                _cceh_record_drop(_cceh_slot_record(&segment->slots[i]));
            }
        }
    }

    _cceh_aligned_free(segment->slots);
    _cceh_aligned_free(segment);
}

bool _cceh_segment_has_space(const cceh_segment_t *segment)
{
    return segment && segment->slot_count < segment->capacity;
}

int _cceh_segment_store_record(cceh_segment_t *segment,
                               cceh_record_t *record,
                               uint32_t *slot_index_out)
{
    uint32_t i;
    uint8_t fingerprint;

    if (!segment || !record) {
        return -1;
    }

    fingerprint = _cceh_fingerprint(record->hashval);
    for (i = 0; i < segment->capacity; i++) {
        if (atomic_load_explicit(&segment->slots[i].state, memory_order_acquire) != CCEH_SLOT_LIVE) {
            atomic_store_explicit(&segment->slots[i].record_ptr, (uintptr_t)record, memory_order_release);
            atomic_store_explicit(&segment->slots[i].fingerprint, fingerprint, memory_order_release);
            atomic_store_explicit(&segment->slots[i].state, CCEH_SLOT_LIVE, memory_order_release);
            segment->slot_count++;
            if (slot_index_out) {
                *slot_index_out = i;
            }
            return 0;
        }
    }

    return 1;
}

int _cceh_segment_find_record(const cceh_segment_t *segment,
                              uint32_t hashval,
                              uint8_t fingerprint,
                              const void *key,
                              uint32_t keylen,
                              uint32_t *slot_index_out)
{
    uint32_t i;

    if (!segment || !key || keylen == 0u) {
        return -1;
    }

    for (i = 0; i < segment->capacity; i++) {
        uint8_t state;
        uint8_t fp;
        cceh_record_t *record;

        state = atomic_load_explicit(&segment->slots[i].state, memory_order_acquire);
        if (state != CCEH_SLOT_LIVE) {
            continue;
        }

        fp = atomic_load_explicit(&segment->slots[i].fingerprint, memory_order_acquire);
        if (fp != fingerprint) {
            continue;
        }

        record = _cceh_slot_record(&segment->slots[i]);
        if (record &&
            record->hashval == hashval &&
            record->keylen == keylen &&
            memcmp(record->key, key, keylen) == 0) {
            if (slot_index_out) {
                *slot_index_out = i;
            }
            return 0;
        }
    }

    return -1;
}

int _cceh_segment_remove_record(cceh_segment_t *segment,
                                uint32_t slot_index,
                                cceh_record_t **record_out)
{
    cceh_record_t *record;

    if (!segment || slot_index >= segment->capacity ||
        atomic_load_explicit(&segment->slots[slot_index].state, memory_order_acquire) != CCEH_SLOT_LIVE) {
        return -1;
    }

    record = _cceh_slot_record(&segment->slots[slot_index]);
    atomic_store_explicit(&segment->slots[slot_index].record_ptr, (uintptr_t)NULL, memory_order_release);
    atomic_store_explicit(&segment->slots[slot_index].fingerprint, 0u, memory_order_release);
    atomic_store_explicit(&segment->slots[slot_index].state, CCEH_SLOT_EMPTY, memory_order_release);
    segment->slot_count--;

    if (record_out) {
        *record_out = record;
    }
    return 0;
}

cceh_index_t *cceh_index_create(uint32_t segment_capacity,
                                uint32_t initial_global_depth)
{
    cceh_index_t *index;
    cceh_directory_t *directory;
    cceh_segment_t *segment;
    uint32_t directory_size;
    uint32_t i;

    if (segment_capacity == 0u) {
        segment_capacity = CCEH_DEFAULT_SEGMENT_CAPACITY;
    }
    if (segment_capacity < CCEH_MIN_SEGMENT_CAPACITY) {
        segment_capacity = CCEH_MIN_SEGMENT_CAPACITY;
    }

    if (initial_global_depth == 0u) {
        initial_global_depth = CCEH_DEFAULT_GLOBAL_DEPTH;
    }
    if (initial_global_depth >= 31u) {
        return NULL;
    }

    /* 第一步: 归一化 segment 容量和目录深度。 */
    directory_size = 1u << initial_global_depth;
    index = (cceh_index_t *)calloc(1, sizeof(cceh_index_t));
    if (!index) {
        return NULL;
    }

    /* 第二步: 创建目录和首个 segment。 */
    directory = _cceh_directory_create(initial_global_depth, directory_size, 1u);
    if (!directory) {
        free(index);
        return NULL;
    }

    segment = _cceh_segment_create(0u, segment_capacity);
    if (!segment) {
        _cceh_directory_drop(directory);
        free(index);
        return NULL;
    }

    /* 第三个阶段: 初始时全部目录槽都指向同一个 segment。 */
    for (i = 0; i < directory_size; i++) {
        directory->segments[i] = segment;
    }

    /* 第四步: 初始化并发控制、epoch 和目录根指针。 */
    atomic_init(&index->n_total, 0u);
    index->segment_capacity = segment_capacity;
    atomic_init(&index->segment_count, 1u);
    atomic_init(&index->persist_epoch, 0u);
    atomic_init(&index->global_epoch, 1u);
    atomic_flag_clear(&index->directory_latch);
    atomic_store_explicit(&index->directory_root, directory, memory_order_release);
    _cceh_publish_changes(index,
                          directory->segments,
                          (size_t)directory->size * sizeof(cceh_segment_t *),
                          segment);
    _cceh_persist_range(directory, sizeof(*directory));
    return index;
}

void cceh_index_drop(cceh_index_t *index)
{
    cceh_directory_t *directory;
    cceh_thread_epoch_t *thread_epoch;
    uint32_t i;

    if (!index) {
        return;
    }

    /*
     * drop 需要先冻结目录根，再把目录中所有唯一 segment 释放掉。
     * 这里借助 epoch 与目录锁，避免并发读线程继续观察到旧结构。
     */
    _cceh_epoch_registry_lock();
    _cceh_directory_lock(index);
    directory = atomic_load_explicit(&index->directory_root, memory_order_acquire);
    atomic_store_explicit(&index->directory_root, NULL, memory_order_release);

    if (directory) {
        for (i = 0; i < directory->size; i++) {
            cceh_segment_t *segment = directory->segments[i];
            uint32_t j;

            if (!segment) {
                continue;
            }

            for (j = i; j < directory->size; j++) {
                if (directory->segments[j] == segment) {
                    directory->segments[j] = NULL;
                }
            }
            _cceh_segment_drop(segment, true);
        }
        _cceh_directory_drop(directory);
    }

    while (index->retired_directories) {
        cceh_retired_directory_t *node = index->retired_directories;

        index->retired_directories = node->next;
        _cceh_directory_drop(node->directory);
        free(node);
    }

    while (index->retired_segments) {
        cceh_retired_segment_t *node = index->retired_segments;

        index->retired_segments = node->next;
        _cceh_segment_drop(node->segment, node->drop_records);
        free(node);
    }

    while (index->retired_records) {
        cceh_retired_record_t *node = index->retired_records;

        index->retired_records = node->next;
        _cceh_record_drop(node->record);
        free(node);
    }

    while (index->thread_epochs) {
        thread_epoch = index->thread_epochs;
        index->thread_epochs = thread_epoch->next;
        atomic_store_explicit(&thread_epoch->owner_index, (uintptr_t)NULL, memory_order_release);
        atomic_store_explicit(&thread_epoch->active, false, memory_order_release);
    }

    _cceh_directory_unlock(index);
    _cceh_epoch_registry_unlock();
    free(index);
}

uint32_t cceh_index_size(const cceh_index_t *index)
{
    return index ? atomic_load_explicit(&index->n_total, memory_order_acquire) : 0u;
}

uint32_t cceh_index_global_depth(const cceh_index_t *index)
{
    cceh_directory_t *directory;

    if (!index) {
        return 0u;
    }

    directory = atomic_load_explicit(&index->directory_root, memory_order_acquire);
    return directory ? directory->global_depth : 0u;
}

uint32_t cceh_index_directory_size(const cceh_index_t *index)
{
    cceh_directory_t *directory;

    if (!index) {
        return 0u;
    }

    directory = atomic_load_explicit(&index->directory_root, memory_order_acquire);
    return directory ? directory->size : 0u;
}

uint32_t cceh_index_segment_count(const cceh_index_t *index)
{
    return index ? atomic_load_explicit(&index->segment_count, memory_order_acquire) : 0u;
}

uint32_t cceh_index_thread_epoch_count(const cceh_index_t *index)
{
    cceh_index_t *mutable_index;
    cceh_thread_epoch_t *node;
    uint32_t count = 0u;

    if (!index) {
        return 0u;
    }

    mutable_index = (cceh_index_t *)index;
    _cceh_epoch_registry_lock();
    _cceh_directory_lock(mutable_index);
    node = mutable_index->thread_epochs;
    while (node) {
        count++;
        node = node->next;
    }
    _cceh_directory_unlock(mutable_index);
    _cceh_epoch_registry_unlock();
    return count;
}

uint32_t cceh_index_segment_local_depth(const cceh_index_t *index,
                                        uint32_t directory_slot)
{
    cceh_directory_t *directory;

    if (!index) {
        return 0u;
    }

    directory = atomic_load_explicit(&index->directory_root, memory_order_acquire);
    if (!directory || directory_slot >= directory->size || !directory->segments[directory_slot]) {
        return 0u;
    }

    return directory->segments[directory_slot]->local_depth;
}
