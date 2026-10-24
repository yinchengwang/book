/*
 * cceh_hash_insert.c
 *
 * Insert path and split logic for the in-memory CCEH index.
 */

#include "cceh_hash_private.h"

static int _cceh_publish_replacement_segment(cceh_index_t *index,
                                             cceh_directory_t *directory,
                                             cceh_segment_t *old_segment,
                                             cceh_segment_t *new_segment)
{
    cceh_directory_t *new_directory;
    uint32_t i;

    if (!index || !directory || !old_segment || !new_segment) {
        return -1;
    }

    new_directory = _cceh_directory_create(directory->global_depth,
                                           directory->size,
                                           directory->version + 1u);
    if (!new_directory) {
        return -1;
    }

    for (i = 0; i < directory->size; i++) {
        new_directory->segments[i] =
            (directory->segments[i] == old_segment) ? new_segment : directory->segments[i];
    }

    _cceh_publish_changes(index,
                          new_segment->slots,
                          (size_t)new_segment->capacity * sizeof(cceh_slot_t),
                          new_segment);
    if (_cceh_publish_directory(index, directory, new_directory) != 0) {
        _cceh_directory_drop(new_directory);
        return -1;
    }
    return 0;
}

static int _cceh_copy_segment_records(cceh_segment_t *source,
                                      cceh_segment_t *left_segment,
                                      cceh_segment_t *right_segment)
{
    uint32_t split_bit;
    uint32_t i;

    if (!source || !left_segment || !right_segment) {
        return -1;
    }

    split_bit = 1u << source->local_depth;
    for (i = 0; i < source->capacity; i++) {
        cceh_record_t *record;
        cceh_segment_t *target;

        if (atomic_load_explicit(&source->slots[i].state, memory_order_acquire) != CCEH_SLOT_LIVE) {
            continue;
        }

        record = (cceh_record_t *)(uintptr_t)atomic_load_explicit(&source->slots[i].record_ptr, memory_order_acquire);
        target = ((record->hashval & split_bit) != 0u) ? right_segment : left_segment;
        if (_cceh_segment_store_record(target, record, NULL) != 0) {
            return -1;
        }
    }

    return 0;
}

int _cceh_split_segment(cceh_index_t *index,
                        cceh_directory_t *directory,
                        uint32_t directory_index,
                        cceh_segment_t *old_segment)
{
    cceh_directory_t *new_directory;
    cceh_segment_t *left_segment;
    cceh_segment_t *right_segment;
    uint32_t new_global_depth;
    uint32_t new_size;
    uint32_t old_local_depth;
    uint32_t split_bit;
    uint32_t i;

    if (!index || !directory || !old_segment) {
        return -1;
    }

    old_local_depth = old_segment->local_depth;
    if (old_local_depth >= 31u) {
        return -1;
    }

    new_global_depth = directory->global_depth;
    new_size = directory->size;
    if (old_local_depth == directory->global_depth) {
        new_global_depth++;
        new_size <<= 1u;
    }

    left_segment = _cceh_segment_create(old_local_depth + 1u, index->segment_capacity);
    if (!left_segment) {
        return -1;
    }

    right_segment = _cceh_segment_create(old_local_depth + 1u, index->segment_capacity);
    if (!right_segment) {
        _cceh_segment_drop(left_segment, false);
        return -1;
    }

    if (_cceh_copy_segment_records(old_segment, left_segment, right_segment) != 0) {
        _cceh_segment_drop(left_segment, false);
        _cceh_segment_drop(right_segment, false);
        return -1;
    }

    new_directory = _cceh_directory_create(new_global_depth, new_size, directory->version + 1u);
    if (!new_directory) {
        _cceh_segment_drop(left_segment, false);
        _cceh_segment_drop(right_segment, false);
        return -1;
    }

    for (i = 0; i < directory->size; i++) {
        new_directory->segments[i] = directory->segments[i];
        if (new_size > directory->size) {
            new_directory->segments[i + directory->size] = directory->segments[i];
        }
    }

    split_bit = 1u << old_local_depth;
    for (i = 0; i < new_size; i++) {
        if (new_directory->segments[i] == old_segment) {
            new_directory->segments[i] = ((i & split_bit) != 0u) ? right_segment : left_segment;
        }
    }

    _cceh_publish_changes(index,
                          left_segment->slots,
                          (size_t)left_segment->capacity * sizeof(cceh_slot_t),
                          left_segment);
    _cceh_publish_changes(index,
                          right_segment->slots,
                          (size_t)right_segment->capacity * sizeof(cceh_slot_t),
                          right_segment);

    if (_cceh_publish_directory(index, directory, new_directory) != 0) {
        _cceh_directory_drop(new_directory);
        _cceh_segment_drop(left_segment, false);
        _cceh_segment_drop(right_segment, false);
        return -1;
    }

    _cceh_retire_segment(index, old_segment, false);
    atomic_fetch_add_explicit(&index->segment_count, 1u, memory_order_acq_rel);
    (void)directory_index;
    return 0;
}

int cceh_index_insert(cceh_index_t *index,
                      const void *key,
                      uint32_t keylen,
                      const void *value,
                      uint32_t valuelen)
{
    uint32_t hashval;
    uint8_t fingerprint;

    if (!index || !key || keylen == 0u) {
        return -1;
    }

    hashval = _cceh_hash_func(key, keylen);
    fingerprint = _cceh_fingerprint(hashval);

    for (;;) {
        cceh_directory_t *directory;
        cceh_segment_t *segment;
        cceh_segment_t *replacement;
        uint32_t directory_index;
        bool changed;
        int clone_result;

        directory = atomic_load_explicit(&index->directory_root, memory_order_acquire);
        if (!directory) {
            return -1;
        }

        directory_index = _cceh_directory_index(directory, hashval);
        segment = directory->segments[directory_index];
        if (!segment) {
            return -1;
        }

        _cceh_segment_pin(segment);
        _cceh_segment_lock(segment);
        _cceh_directory_lock(index);

        if (atomic_load_explicit(&index->directory_root, memory_order_acquire) != directory ||
            atomic_load_explicit(&segment->retired, memory_order_acquire) ||
            directory->segments[directory_index] != segment) {
            _cceh_directory_unlock(index);
            _cceh_segment_unlock(segment);
            _cceh_segment_unpin(segment);
            continue;
        }

        if (_cceh_segment_find_record(segment, hashval, fingerprint, key, keylen, NULL) == 0) {
            _cceh_directory_unlock(index);
            _cceh_segment_unlock(segment);
            _cceh_segment_unpin(segment);
            return 1;
        }

        if (_cceh_segment_has_space(segment)) {
            clone_result = _cceh_segment_clone_with_change(segment,
                                                           hashval,
                                                           fingerprint,
                                                           key,
                                                           keylen,
                                                           value,
                                                           valuelen,
                                                           false,
                                                           true,
                                                           &changed,
                                                           &replacement,
                                                           NULL);
            if (clone_result != 0 || !changed) {
                _cceh_directory_unlock(index);
                _cceh_segment_unlock(segment);
                _cceh_segment_unpin(segment);
                return -1;
            }

            if (_cceh_publish_replacement_segment(index, directory, segment, replacement) != 0) {
                _cceh_segment_drop(replacement, true);
                _cceh_directory_unlock(index);
                _cceh_segment_unlock(segment);
                _cceh_segment_unpin(segment);
                return -1;
            }

            atomic_fetch_add_explicit(&index->n_total, 1u, memory_order_acq_rel);
            _cceh_retire_segment(index, segment, false);
            _cceh_directory_unlock(index);
            _cceh_segment_unlock(segment);
            _cceh_segment_unpin(segment);

            _cceh_directory_lock(index);
            _cceh_reclaim_retired(index);
            _cceh_directory_unlock(index);
            return 0;
        }

        if (_cceh_split_segment(index, directory, directory_index, segment) != 0) {
            _cceh_directory_unlock(index);
            _cceh_segment_unlock(segment);
            _cceh_segment_unpin(segment);
            return -1;
        }

        _cceh_directory_unlock(index);
        _cceh_segment_unlock(segment);
        _cceh_segment_unpin(segment);

        _cceh_directory_lock(index);
        _cceh_reclaim_retired(index);
        _cceh_directory_unlock(index);
    }
}

int cceh_index_upsert(cceh_index_t *index,
                      const void *key,
                      uint32_t keylen,
                      const void *value,
                      uint32_t valuelen)
{
    uint32_t hashval;
    uint8_t fingerprint;

    if (!index || !key || keylen == 0u) {
        return -1;
    }

    hashval = _cceh_hash_func(key, keylen);
    fingerprint = _cceh_fingerprint(hashval);

    for (;;) {
        cceh_directory_t *directory;
        cceh_segment_t *segment;
        cceh_segment_t *replacement;
        cceh_record_t *replaced_record;
        uint32_t directory_index;
        bool changed;
        int clone_result;

        directory = atomic_load_explicit(&index->directory_root, memory_order_acquire);
        if (!directory) {
            return -1;
        }

        directory_index = _cceh_directory_index(directory, hashval);
        segment = directory->segments[directory_index];
        if (!segment) {
            return -1;
        }

        _cceh_segment_pin(segment);
        _cceh_segment_lock(segment);
        _cceh_directory_lock(index);

        if (atomic_load_explicit(&index->directory_root, memory_order_acquire) != directory ||
            atomic_load_explicit(&segment->retired, memory_order_acquire) ||
            directory->segments[directory_index] != segment) {
            _cceh_directory_unlock(index);
            _cceh_segment_unlock(segment);
            _cceh_segment_unpin(segment);
            continue;
        }

        clone_result = _cceh_segment_clone_with_change(segment,
                                                       hashval,
                                                       fingerprint,
                                                       key,
                                                       keylen,
                                                       value,
                                                       valuelen,
                                                       false,
                                                       false,
                                                       &changed,
                                                       &replacement,
                                                       &replaced_record);
        if (clone_result == 1) {
            _cceh_directory_unlock(index);
            _cceh_segment_unlock(segment);
            _cceh_segment_unpin(segment);
            return cceh_index_insert(index, key, keylen, value, valuelen);
        }
        if (clone_result != 0 || !changed) {
            _cceh_directory_unlock(index);
            _cceh_segment_unlock(segment);
            _cceh_segment_unpin(segment);
            return -1;
        }

        if (_cceh_publish_replacement_segment(index, directory, segment, replacement) != 0) {
            _cceh_segment_drop(replacement, true);
            _cceh_directory_unlock(index);
            _cceh_segment_unlock(segment);
            _cceh_segment_unpin(segment);
            return -1;
        }

        _cceh_retire_segment(index, segment, false);
        _cceh_retire_record(index, replaced_record);
        _cceh_directory_unlock(index);
        _cceh_segment_unlock(segment);
        _cceh_segment_unpin(segment);

        _cceh_directory_lock(index);
        _cceh_reclaim_retired(index);
        _cceh_directory_unlock(index);
        return 0;
    }
}
