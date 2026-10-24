/**
 * @file blob_reader_table.c
 * @brief Shared reader count table implementation
 */
#include "blob_reader_table.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief DJB2 hash function
 */
static size_t hash_chunk_id(const uint8_t chunk_id[32]) {
    uint32_t h = 5381;
    for (int i = 0; i < 32; i++) {
        h = ((h << 5) + h) ^ chunk_id[i];
    }
    return h;
}

reader_table_t *reader_table_create(void) {
    reader_table_t *table = (reader_table_t *)calloc(1, sizeof(reader_table_t));
    if (!table) return NULL;

    table->size = READER_TABLE_INITIAL_SIZE;
    table->entries = (reader_entry_t *)calloc(table->size, sizeof(reader_entry_t));
    if (!table->entries) {
        free(table);
        return NULL;
    }

    return table;
}

void reader_table_destroy(reader_table_t *table) {
    if (!table) return;
    free(table->entries);
    free(table);
}

int reader_table_expand(reader_table_t *table) {
    if (!table) return -1;

    /* Check load factor */
    double load_factor = (double)table->count / (double)table->size;
    if (load_factor < READER_TABLE_LOAD_FACTOR_THRESHOLD) {
        return 0;
    }

    /* New size: double */
    size_t new_size = table->size * 2;
    reader_entry_t *new_entries = (reader_entry_t *)calloc(new_size, sizeof(reader_entry_t));
    if (!new_entries) {
        return -1;
    }

    /* Rehash all entries */
    for (size_t i = 0; i < table->size; i++) {
        if (!table->entries[i].occupied) {
            continue;
        }

        size_t idx = hash_chunk_id(table->entries[i].chunk_id) % new_size;

        for (size_t j = 0; j < new_size; j++) {
            size_t pos = (idx + j) % new_size;

            if (!new_entries[pos].occupied) {
                memcpy(new_entries[pos].chunk_id, table->entries[i].chunk_id, 32);
                new_entries[pos].reader_count = table->entries[i].reader_count;
                new_entries[pos].occupied = true;
                break;
            }
        }
    }

    free(table->entries);
    table->entries = new_entries;
    table->size = new_size;

    return 0;
}

uint32_t reader_table_inc(reader_table_t *table, const uint8_t chunk_id[32]) {
    if (!table || !chunk_id) return 0;

    /* Expand if needed */
    if (reader_table_expand(table) != 0) {
        return 0;
    }

    size_t idx = hash_chunk_id(chunk_id) % table->size;

    for (size_t i = 0; i < table->size; i++) {
        size_t pos = (idx + i) % table->size;

        if (!table->entries[pos].occupied) {
            memcpy(table->entries[pos].chunk_id, chunk_id, 32);
            table->entries[pos].reader_count = 1;
            table->entries[pos].occupied = true;
            table->count++;
            return 1;
        }

        if (memcmp(table->entries[pos].chunk_id, chunk_id, 32) == 0) {
            table->entries[pos].reader_count++;
            return table->entries[pos].reader_count;
        }
    }

    return 0;
}

uint32_t reader_table_dec(reader_table_t *table, const uint8_t chunk_id[32]) {
    if (!table || !chunk_id) return 0;

    size_t idx = hash_chunk_id(chunk_id) % table->size;

    for (size_t i = 0; i < table->size; i++) {
        size_t pos = (idx + i) % table->size;

        if (!table->entries[pos].occupied) {
            continue;
        }

        if (memcmp(table->entries[pos].chunk_id, chunk_id, 32) == 0) {
            if (table->entries[pos].reader_count > 0) {
                table->entries[pos].reader_count--;
            }
            return table->entries[pos].reader_count;
        }
    }

    return 0;
}

uint32_t reader_table_get(reader_table_t *table, const uint8_t chunk_id[32]) {
    if (!table || !chunk_id) return 0;

    size_t idx = hash_chunk_id(chunk_id) % table->size;

    for (size_t i = 0; i < table->size; i++) {
        size_t pos = (idx + i) % table->size;

        if (!table->entries[pos].occupied) {
            continue;
        }

        if (memcmp(table->entries[pos].chunk_id, chunk_id, 32) == 0) {
            return table->entries[pos].reader_count;
        }
    }

    return 0;
}
