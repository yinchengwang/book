/*
 * cceh_hash_delete.c
 *
 * Delete path for the in-memory CCEH index.
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

int cceh_index_delete(cceh_index_t *index,
                      const void *key,
                      uint32_t keylen)
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
        cceh_record_t *record;
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
                                                       NULL,
                                                       0u,
                                                       true,
                                                       false,
                                                       &changed,
                                                       &replacement,
                                                       &record);
        if (clone_result == 1) {
            _cceh_directory_unlock(index);
            _cceh_segment_unlock(segment);
            _cceh_segment_unpin(segment);
            return -1;
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

        atomic_fetch_sub_explicit(&index->n_total, 1u, memory_order_acq_rel);
        _cceh_retire_segment(index, segment, false);
        _cceh_retire_record(index, record);
        _cceh_directory_unlock(index);
        _cceh_segment_unlock(segment);
        _cceh_segment_unpin(segment);

        _cceh_directory_lock(index);
        _cceh_reclaim_retired(index);
        _cceh_directory_unlock(index);
        return 0;
    }
}
