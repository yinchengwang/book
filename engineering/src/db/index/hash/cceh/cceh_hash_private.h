/*
 * cceh_hash_private.h
 *
 * Internal declarations for the in-memory CCEH index.
 */

#ifndef CCEH_HASH_PRIVATE_H
#define CCEH_HASH_PRIVATE_H

#include <db/index/hash/cceh.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CCEH_CACHELINE_SIZE 64u
#define CCEH_SLOT_EMPTY     0u
#define CCEH_SLOT_LIVE      1u

typedef struct cceh_record {
    uint32_t hashval;   /* 缓存哈希值，避免 split / lookup 时重复计算。 */
    void    *key;       /* key 的堆拷贝。 */
    uint32_t keylen;    /* key 字节长度。 */
    void    *value;     /* value 的堆拷贝。 */
    uint32_t valuelen;  /* value 字节长度。 */
} cceh_record_t;

/*
 * 段内槽位。
 * fingerprint 用于快速排除大部分不匹配 key，record_ptr 指向真实记录。
 */
typedef struct cceh_slot {
    atomic_uchar     fingerprint;
    atomic_uchar     state;
    uint16_t         reserved16;
    uint32_t         reserved32;
    atomic_uintptr_t record_ptr;
} cceh_slot_t;

/*
 * CCEH 的 segment 是 split 的基本单位。
 * local_depth 决定它被目录中多少个槽共享，slots 保存该段内的实际记录。
 */
typedef struct cceh_segment {
    atomic_flag  latch;
    atomic_uint  pin_count;
    atomic_uint  version;
    atomic_bool  retired;
    uint32_t     persist_epoch;
    uint32_t     local_depth;
    uint32_t     slot_count;
    uint32_t     capacity;
    cceh_slot_t *slots;
} cceh_segment_t;

/* 全局目录：directory slot -> segment 指针映射表。 */
typedef struct cceh_directory {
    uint32_t         version;
    uint32_t         global_depth;
    uint32_t         size;
    uint32_t         reserved;
    cceh_segment_t **segments;
} cceh_directory_t;

/* 旧目录退休链表，用于 epoch 保护下的延迟回收。 */
typedef struct cceh_retired_directory {
    cceh_directory_t               *directory;
    uint32_t                        retire_epoch;
    struct cceh_retired_directory  *next;
} cceh_retired_directory_t;

/* 旧 segment 退休链表。 */
typedef struct cceh_retired_segment {
    cceh_segment_t               *segment;
    uint32_t                      retire_epoch;
    bool                          drop_records;
    uint8_t                       reserved[3];
    struct cceh_retired_segment  *next;
} cceh_retired_segment_t;

/* 已删除记录的退休链表。 */
typedef struct cceh_retired_record {
    cceh_record_t               *record;
    uint32_t                      retire_epoch;
    struct cceh_retired_record  *next;
} cceh_retired_record_t;

/* 每个线程在读路径上的 epoch 注册项。 */
typedef struct cceh_thread_epoch {
    _Atomic(uintptr_t)             thread_id;
    atomic_uintptr_t               owner_index;
    atomic_uint                    epoch;
    atomic_bool                    active;
    struct cceh_thread_epoch      *next;
} cceh_thread_epoch_t;

struct cceh_index {
    atomic_uint                  n_total;              /* 活跃记录数。 */
    uint32_t                     segment_capacity;     /* 每个 segment 的固定容量。 */
    atomic_uint                  segment_count;        /* 已分配 segment 数。 */
    atomic_uint                  persist_epoch;        /* 持久化发布序号。 */
    atomic_uint                  global_epoch;         /* 并发读保护用全局 epoch。 */
    atomic_flag                  directory_latch;      /* 目录扩容/替换锁。 */
    _Atomic(cceh_directory_t *)  directory_root;       /* 当前生效目录。 */
    cceh_thread_epoch_t         *thread_epochs;        /* 线程 epoch 注册表。 */
    cceh_retired_directory_t    *retired_directories;  /* 待回收旧目录。 */
    cceh_retired_segment_t      *retired_segments;     /* 待回收旧 segment。 */
    cceh_retired_record_t       *retired_records;      /* 待回收旧记录。 */
};

uint32_t _cceh_hash_func(const void *key, uint32_t keylen);
uint8_t  _cceh_fingerprint(uint32_t hashval);
uint32_t _cceh_directory_index(const cceh_directory_t *directory, uint32_t hashval);

void    *_cceh_aligned_alloc(size_t alignment, size_t size);
void     _cceh_aligned_free(void *ptr);
void     _cceh_flush_range(const void *addr, size_t len);
void     _cceh_drain_writes(void);
void     _cceh_persist_range(const void *addr, size_t len);
void     _cceh_publish_changes(cceh_index_t *index,
                               const void *addr,
                               size_t len,
                               cceh_segment_t *segment);

void     _cceh_directory_lock(cceh_index_t *index);
void     _cceh_directory_unlock(cceh_index_t *index);
void     _cceh_segment_lock(cceh_segment_t *segment);
void     _cceh_segment_unlock(cceh_segment_t *segment);
void     _cceh_segment_pin(cceh_segment_t *segment);
void     _cceh_segment_unpin(cceh_segment_t *segment);
void     _cceh_reader_enter(cceh_index_t *index);
void     _cceh_reader_exit(cceh_index_t *index);
void     _cceh_segment_begin_write(cceh_segment_t *segment);
void     _cceh_segment_end_write(cceh_segment_t *segment);
uint32_t _cceh_advance_global_epoch(cceh_index_t *index);
uint32_t _cceh_min_active_epoch(const cceh_index_t *index);

cceh_directory_t *_cceh_directory_create(uint32_t global_depth,
                                         uint32_t size,
                                         uint32_t version);
void _cceh_directory_drop(cceh_directory_t *directory);
int  _cceh_publish_directory(cceh_index_t *index,
                             cceh_directory_t *old_directory,
                             cceh_directory_t *new_directory);

void _cceh_retire_directory(cceh_index_t *index, cceh_directory_t *directory);
void _cceh_retire_segment(cceh_index_t *index,
                          cceh_segment_t *segment,
                          bool drop_records);
void _cceh_retire_record(cceh_index_t *index, cceh_record_t *record);
void _cceh_reclaim_retired(cceh_index_t *index);

cceh_record_t *_cceh_record_create(uint32_t hashval,
                                   const void *key,
                                   uint32_t keylen,
                                   const void *value,
                                   uint32_t valuelen);
void _cceh_record_drop(cceh_record_t *record);

cceh_segment_t *_cceh_segment_create(uint32_t local_depth, uint32_t capacity);
void _cceh_segment_drop(cceh_segment_t *segment, bool drop_records);
bool _cceh_segment_has_space(const cceh_segment_t *segment);
int _cceh_segment_store_record(cceh_segment_t *segment,
                               cceh_record_t *record,
                               uint32_t *slot_index_out);
int _cceh_segment_find_record(const cceh_segment_t *segment,
                              uint32_t hashval,
                              uint8_t fingerprint,
                              const void *key,
                              uint32_t keylen,
                              uint32_t *slot_index_out);
int _cceh_segment_remove_record(cceh_segment_t *segment,
                                uint32_t slot_index,
                                cceh_record_t **record_out);
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
                                    cceh_record_t **replaced_record_out);

int _cceh_split_segment(cceh_index_t *index,
                        cceh_directory_t *directory,
                        uint32_t directory_index,
                        cceh_segment_t *old_segment);

#endif /* CCEH_HASH_PRIVATE_H */
