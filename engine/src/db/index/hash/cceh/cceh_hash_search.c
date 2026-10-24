/*
 * cceh_hash_search.c
 *
 * Lock-free lookup path for the in-memory CCEH index.
 */

#include "cceh_hash_private.h"

int cceh_index_lookup(const cceh_index_t *index,
                      const void *key,
                      uint32_t keylen,
                      void **value_out,
                      uint32_t *valuelen_out)
{
    cceh_index_t *mutable_index;
    uint32_t hashval;
    uint8_t fingerprint;
    int result;

    if (!index || !key || keylen == 0u) {
        return -1;
    }

    mutable_index = (cceh_index_t *)index;
    hashval = _cceh_hash_func(key, keylen);
    fingerprint = _cceh_fingerprint(hashval);
    result = -1;

    _cceh_reader_enter(mutable_index);
    for (;;) {
        cceh_directory_t *directory;
        cceh_segment_t *segment;
        uint32_t directory_index;
        uint32_t version_before;
        uint32_t version_after;
        uint32_t slot_index;
        cceh_record_t *record;

        directory = atomic_load_explicit(&mutable_index->directory_root, memory_order_acquire);
        if (!directory) {
            result = -1;
            break;
        }

        directory_index = _cceh_directory_index(directory, hashval);
        segment = directory->segments[directory_index];
        if (!segment) {
            result = -1;
            break;
        }

        _cceh_segment_pin(segment);
        version_before = atomic_load_explicit(&segment->version, memory_order_acquire);
        if ((version_before & 1u) != 0u) {
            _cceh_segment_unpin(segment);
            continue;
        }

        if (_cceh_segment_find_record(segment, hashval, fingerprint, key, keylen, &slot_index) == 0) {
            record = (cceh_record_t *)(uintptr_t)atomic_load_explicit(&segment->slots[slot_index].record_ptr,
                                                                      memory_order_acquire);
            if (record) {
                if (value_out) {
                    *value_out = record->value;
                }
                if (valuelen_out) {
                    *valuelen_out = record->valuelen;
                }
                result = 0;
            } else {
                result = -1;
            }
        } else {
            result = -1;
        }

        version_after = atomic_load_explicit(&segment->version, memory_order_acquire);
        _cceh_segment_unpin(segment);
        if (version_before == version_after && (version_after & 1u) == 0u) {
            break;
        }
    }

    _cceh_reader_exit(mutable_index);
    return result;
}
