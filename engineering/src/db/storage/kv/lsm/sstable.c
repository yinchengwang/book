/**
 * @file sstable.c
 * @brief SSTable 文件格式实现
 */

#include "db/storage/kv/lsm/sstable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define SSTABLE_MAGIC 0x53535442  /* "SSTB" */
#define SSTABLE_VERSION 1
#define SSTABLE_HEADER_SIZE 64
#define SSTABLE_FOOTER_SIZE 16

/* ========================================================================
 * 头文件布局
 * ======================================================================== */

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t min_key;
    uint64_t max_key;
    uint64_t min_seq;
    uint64_t max_seq;
    uint32_t num_entries;
    uint64_t index_offset;
    uint64_t bloom_offset;
    uint32_t reserved;
} sstable_header_t;

typedef struct {
    uint32_t bloom_size;
    uint32_t checksum;
    uint64_t reserved;
} sstable_footer_t;

typedef struct {
    uint32_t key_size;
    uint32_t value_size;
    uint64_t seq;
    uint8_t op;
} sstable_entry_header_t;

#pragma pack(pop)

/* ========================================================================
 * 工具函数
 * ======================================================================== */

static uint32_t crc32(const void *data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *ptr = (const uint8_t *)data;

    for (size_t i = 0; i < size; i++) {
        crc ^= ptr[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    return ~crc;
}

static int64_t get_file_size(FILE *f) {
    long pos = ftell(f);
    fseek(f, 0, SEEK_END);
    int64_t size = ftell(f);
    fseek(f, pos, SEEK_SET);
    return size;
}

/* ========================================================================
 * 写入实现
 * ======================================================================== */

int sstable_write(const char *path,
                  const lsm_entry_t *entries,
                  size_t count,
                  const lsm_bloom_filter_t *bloom,
                  uint64_t min_key,
                  uint64_t max_key) {
    if (!path || !entries || count == 0) return -1;

    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    /* 写入 Header */
    sstable_header_t header = {0};
    header.magic = SSTABLE_MAGIC;
    header.version = SSTABLE_VERSION;
    header.min_key = min_key;
    header.max_key = max_key;
    header.min_seq = entries[0].seq;
    header.max_seq = entries[count - 1].seq;
    header.num_entries = (uint32_t)count;

    /* 暂定偏移量，后续更新 */
    header.index_offset = SSTABLE_HEADER_SIZE;
    header.bloom_offset = 0;

    fwrite(&header, sizeof(header), 1, f);

    /* 写入 Data Section */
    uint64_t data_offset = SSTABLE_HEADER_SIZE;
    uint64_t *index_entries = (uint64_t *)malloc(count * sizeof(uint64_t));
    if (!index_entries) {
        fclose(f);
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        const lsm_entry_t *e = &entries[i];
        index_entries[i] = data_offset;

        /* 写入 Entry Header */
        sstable_entry_header_t eh = {
            .key_size = e->key_size,
            .value_size = e->value_size,
            .seq = e->seq,
            .op = (uint8_t)e->op
        };
        fwrite(&eh, sizeof(eh), 1, f);
        data_offset += sizeof(eh);

        /* 写入 Key */
        fwrite(e->key, e->key_size, 1, f);
        data_offset += e->key_size;

        /* 写入 Value */
        if (e->value && e->value_size > 0) {
            fwrite(e->value, e->value_size, 1, f);
            data_offset += e->value_size;
        }
    }

    /* 更新 Header 中的 index_offset */
    header.index_offset = data_offset;
    fseek(f, 0, SEEK_SET);
    fwrite(&header, sizeof(header), 1, f);

    /* 写入 Index Section */
    for (size_t i = 0; i < count; i++) {
        const lsm_entry_t *e = &entries[i];
        uint32_t key_size = e->key_size;
        fwrite(&key_size, sizeof(key_size), 1, f);
        fwrite(e->key, e->key_size, 1, f);
        fwrite(&index_entries[i], sizeof(index_entries[i]), 1, f);
    }

    /* 写入 Bloom Filter */
    header.bloom_offset = ftell(f);

    if (bloom && bloom->bits) {
        fwrite(bloom->bits, bloom->size, 1, f);
    }

    /* 写入 Footer */
    sstable_footer_t footer = {0};
    footer.bloom_size = bloom ? (uint32_t)bloom->size : 0;
    footer.checksum = 0;
    fwrite(&footer, sizeof(footer), 1, f);

    free(index_entries);
    fclose(f);
    return 0;
}

/* ========================================================================
 * 读取实现
 * ======================================================================== */

int sstable_read(const char *path,
                 lsm_entry_t **out_entries,
                 size_t *out_count,
                 lsm_bloom_filter_t **out_bloom) {
    if (!path || !out_entries || !out_count) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    /* 读取 Header */
    sstable_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    if (header.magic != SSTABLE_MAGIC) {
        fclose(f);
        return -1;
    }

    size_t count = header.num_entries;
    lsm_entry_t *entries = (lsm_entry_t *)calloc(count, sizeof(lsm_entry_t));
    if (!entries) {
        fclose(f);
        return -1;
    }

    /* 读取 Data Section */
    for (size_t i = 0; i < count; i++) {
        sstable_entry_header_t eh;
        if (fread(&eh, sizeof(eh), 1, f) != 1) {
            free(entries);
            fclose(f);
            return -1;
        }

        entries[i].key_size = eh.key_size;
        entries[i].value_size = eh.value_size;
        entries[i].seq = eh.seq;
        entries[i].op = eh.op;

        entries[i].key = malloc(eh.key_size);
        if (fread(entries[i].key, eh.key_size, 1, f) != 1) {
            for (size_t j = 0; j < i; j++) {
                free(entries[j].key);
                free(entries[j].value);
            }
            free(entries);
            fclose(f);
            return -1;
        }

        if (eh.value_size > 0) {
            entries[i].value = malloc(eh.value_size);
            if (fread(entries[i].value, eh.value_size, 1, f) != 1) {
                free(entries[i].key);
                for (size_t j = 0; j < i; j++) {
                    free(entries[j].key);
                    free(entries[j].value);
                }
                free(entries);
                fclose(f);
                return -1;
            }
        }
    }

    *out_entries = entries;
    *out_count = count;

    /* 读取 Bloom Filter（如果需要） */
    if (out_bloom && header.bloom_offset > 0) {
        fseek(f, header.bloom_offset, SEEK_SET);
        sstable_footer_t footer;
        if (fread(&footer, sizeof(footer), 1, f) == 1 && footer.bloom_size > 0) {
            lsm_bloom_filter_t *bf = lsm_bloom_create(footer.bloom_size, LSM_BLOOM_FILTER_HASH_COUNT);
            if (bf) {
                fseek(f, header.bloom_offset, SEEK_SET);
                fread(bf->bits, footer.bloom_size, 1, f);
                *out_bloom = bf;
            }
        }
    }

    fclose(f);
    return 0;
}

int sstable_read_meta(const char *path, lsm_sstable_meta_t *out_meta) {
    if (!path || !out_meta) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    sstable_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    out_meta->min_key = header.min_key;
    out_meta->max_key = header.max_key;
    out_meta->min_seq = header.min_seq;
    out_meta->max_seq = header.max_seq;
    out_meta->file_size = get_file_size(f);

    fclose(f);
    return 0;
}

int sstable_search(const char *path,
                   const void *key, size_t key_size,
                   void **out_value, size_t *out_size) {
    if (!path || !key) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    sstable_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    /* 线性扫描（简化实现，实际应该用索引） */
    for (uint32_t i = 0; i < header.num_entries; i++) {
        sstable_entry_header_t eh;
        if (fread(&eh, sizeof(eh), 1, f) != 1) {
            fclose(f);
            return -1;
        }

        void *file_key = malloc(eh.key_size);
        if (fread(file_key, eh.key_size, 1, f) != 1) {
            free(file_key);
            fclose(f);
            return -1;
        }

        if (eh.key_size == key_size && memcmp(file_key, key, key_size) == 0) {
            free(file_key);

            if (eh.op == LSM_OP_DELETE) {
                fclose(f);
                return 1; /* 已删除 */
            }

            if (out_value && out_size) {
                if (eh.value_size > 0) {
                    *out_value = malloc(eh.value_size);
                    if (fread(*out_value, eh.value_size, 1, f) != 1) {
                        free(*out_value);
                        fclose(f);
                        return -1;
                    }
                    *out_size = eh.value_size;
                } else {
                    *out_value = NULL;
                    *out_size = 0;
                }
            }

            fclose(f);
            return 0; /* 找到 */
        }

        free(file_key);

        /* 跳过 value */
        if (eh.value_size > 0) {
            fseek(f, eh.value_size, SEEK_CUR);
        }
    }

    fclose(f);
    return 1; /* 未找到 */
}

bool sstable_might_contain(const char *path,
                          const void *key, size_t key_size) {
    if (!path || !key) return false;

    FILE *f = fopen(path, "rb");
    if (!f) return false;

    sstable_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return false;
    }

    /* 读取布隆过滤器 */
    if (header.bloom_offset > 0) {
        fseek(f, header.bloom_offset, SEEK_SET);
        sstable_footer_t footer;
        if (fread(&footer, sizeof(footer), 1, f) == 1 && footer.bloom_size > 0) {
            uint8_t *bits = (uint8_t *)malloc(footer.bloom_size);
            if (bits && fread(bits, footer.bloom_size, 1, f) == 1) {
                /* 构造临时布隆过滤器来检查 */
                lsm_bloom_filter_t temp_bloom = {
                    .bits = bits,
                    .size = footer.bloom_size,
                    .num_hashes = LSM_BLOOM_FILTER_HASH_COUNT
                };
                bool result = lsm_bloom_might_contain(&temp_bloom, key, key_size);
                free(bits);
                fclose(f);
                return result;
            }
            free(bits);
        }
    }

    fclose(f);
    return true; /* 无法确定，返回可能存在 */
}

int64_t sstable_file_size(const char *path) {
    if (!path) return -1;

    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return st.st_size;
}

size_t sstable_batch_search(const char *path,
                            const void **keys,
                            const size_t *key_sizes,
                            size_t count,
                            void **results,
                            size_t *result_sizes) {
    if (!path || !keys || !results) return 0;

    size_t found = 0;
    for (size_t i = 0; i < count; i++) {
        void *value = NULL;
        size_t value_size = 0;
        int ret = sstable_search(path, keys[i], key_sizes[i], &value, &value_size);

        if (ret == 0) {
            results[found] = value;
            if (result_sizes) result_sizes[found] = value_size;
            found++;
        }
    }

    return found;
}
