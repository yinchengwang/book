/**
 * @file blob_reader_table.h
 * @brief Shared reader count table for active chunk readers
 *
 * Provides thread-safe reader counting to protect chunks from GC
 * while they are being read. Includes hash table expansion when
 * load factor exceeds threshold.
 */
#ifndef BLOB_READER_TABLE_H
#define BLOB_READER_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define READER_TABLE_INITIAL_SIZE 1024
#define READER_TABLE_LOAD_FACTOR_THRESHOLD 0.75

/**
 * @brief Reader count entry
 */
typedef struct reader_entry_s {
    uint8_t  chunk_id[32];      /**< Chunk ID */
    uint32_t reader_count;      /**< Current reader count */
    bool     occupied;          /**< Whether slot is occupied */
} reader_entry_t;

/**
 * @brief Reader count table
 */
typedef struct reader_table_s {
    reader_entry_t *entries;
    size_t          size;
    size_t          count;          /**< Number of occupied entries */
} reader_table_t;

/**
 * @brief Create a reader count table
 */
reader_table_t *reader_table_create(void);

/**
 * @brief Destroy a reader count table
 */
void reader_table_destroy(reader_table_t *table);

/**
 * @brief Increment reader count for a chunk
 * @return New count, or 0 on failure
 */
uint32_t reader_table_inc(reader_table_t *table, const uint8_t chunk_id[32]);

/**
 * @brief Decrement reader count for a chunk
 * @return New count, or 0 if not found
 */
uint32_t reader_table_dec(reader_table_t *table, const uint8_t chunk_id[32]);

/**
 * @brief Get current reader count for a chunk
 * @return Current count, or 0 if not found
 */
uint32_t reader_table_get(reader_table_t *table, const uint8_t chunk_id[32]);

/**
 * @brief Expand table if load factor exceeds threshold
 * @return 0 on success, -1 on failure
 */
int reader_table_expand(reader_table_t *table);

#endif /* BLOB_READER_TABLE_H */
