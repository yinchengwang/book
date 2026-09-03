/**
 * @file main.c
 * @brief SSTable 演示程序
 *
 * 展示 SSTable 文件格式：
 * - Data Block：存储键值对数据
 * - Index Block：索引数据块位置
 * - Filter Block：布隆过滤器快速判断键是否存在
 * - Footer：元数据指针
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * 常量定义
 * ============================================================================ */

#define BLOCK_SIZE 4096           /* 块大小：4KB */
#define MAX_KEYS 16               /* 最大键数量 */
#define MAX_KEY_LEN 64            /* 最大键长度 */
#define MAX_VALUE_LEN 256         /* 最大值长度 */

/* ============================================================================
 * 数据结构
 * ============================================================================ */

/* 数据块头 */
typedef struct {
    uint32_t num_entries;         /* 条目数量 */
    uint32_t data_size;           /* 数据大小 */
} block_header_t;

/* 键值条目 */
typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
    uint32_t key_len;
    uint32_t value_len;
} kv_entry_t;

/* Data Block（演示用，padding 固定为 1024 字节） */
typedef struct {
    block_header_t header;
    kv_entry_t entries[MAX_KEYS];
    uint8_t padding[1024];  /* 演示用固定 padding，实际生产环境动态计算 */
} data_block_t;

/* Index Block 条目：指向 Data Block */
typedef struct {
    char last_key[MAX_KEY_LEN];   /* 最后一个键 */
    uint32_t block_offset;        /* 块在文件中的偏移 */
    uint32_t block_size;          /* 块大小 */
} index_entry_t;

/* Index Block */
typedef struct {
    block_header_t header;
    index_entry_t entries[MAX_KEYS];
} index_block_t;

/* Bloom Filter（简化版） */
typedef struct {
    uint32_t num_hashes;          /* 哈希函数数量 */
    uint64_t bitmask;             /* 位数组（简化：64位） */
} bloom_filter_t;

/* Filter Block */
typedef struct {
    block_header_t header;
    bloom_filter_t filters[4];    /* 支持4个data block */
} filter_block_t;

/* Footer：指向各区域的指针 */
typedef struct {
    uint64_t index_offset;        /* Index Block 偏移 */
    uint64_t index_size;          /* Index Block 大小 */
    uint64_t filter_offset;       /* Filter Block 偏移 */
    uint64_t filter_size;         /* Filter Block 大小 */
    uint64_t magic;               /* 魔数：0x53455441424C4500 "SSTABLE" */
} sstable_footer_t;

/* ============================================================================
 * Bloom Filter 操作
 * ============================================================================ */

/* 简单的哈希函数 */
static uint64_t hash(const char *key, uint32_t seed) {
    uint64_t h = 5381 + seed;
    while (*key) {
        h = h * 33 ^ (uint64_t)(*key++);
    }
    return h;
}

/* 添加键到布隆过滤器 */
static void bloom_add(bloom_filter_t *bf, const char *key) {
    for (uint32_t i = 0; i < bf->num_hashes; i++) {
        uint64_t h = hash(key, i);
        bf->bitmask |= (1ULL << (h % 64));
    }
}

/* 检查键是否可能在过滤器中 */
static bool bloom_might_contain(const bloom_filter_t *bf, const char *key) {
    for (uint32_t i = 0; i < bf->num_hashes; i++) {
        uint64_t h = hash(key, i);
        if ((bf->bitmask & (1ULL << (h % 64))) == 0) {
            return false;
        }
    }
    return true;
}

/* ============================================================================
 * 主程序
 * ============================================================================ */

int main(void) {
    printf("[sstable] SSTable 文件格式演示\n");
    printf("=================================\n\n");

    /* ------------------------------------------------------------------------
     * 步骤1：创建 Data Block
     * --------------------------------------------------------------------- */
    printf("[sstable] 步骤1：创建 Data Block\n");
    data_block_t data_block;
    memset(&data_block, 0, sizeof(data_block));

    data_block.header.num_entries = 3;
    data_block.header.data_size = sizeof(kv_entry_t) * 3;

    /* 添加键值对 */
    strcpy(data_block.entries[0].key, "name");
    strcpy(data_block.entries[0].value, "Alice");
    data_block.entries[0].key_len = 4;
    data_block.entries[0].value_len = 5;

    strcpy(data_block.entries[1].key, "age");
    strcpy(data_block.entries[1].value, "30");
    data_block.entries[1].key_len = 3;
    data_block.entries[1].value_len = 2;

    strcpy(data_block.entries[2].key, "city");
    strcpy(data_block.entries[2].value, "Beijing");
    data_block.entries[2].key_len = 4;
    data_block.entries[2].value_len = 7;

    printf("  - Data Block 大小: %zu 字节\n", sizeof(data_block_t));
    printf("  - 条目数量: %u\n", data_block.header.num_entries);
    printf("  - 键: name, age, city\n\n");

    /* ------------------------------------------------------------------------
     * 步骤2：创建 Index Block
     * --------------------------------------------------------------------- */
    printf("[sstable] 步骤2：创建 Index Block\n");
    index_block_t index_block;
    memset(&index_block, 0, sizeof(index_block));

    index_block.header.num_entries = 1;
    strcpy(index_block.entries[0].last_key, "city");
    index_block.entries[0].block_offset = 0;
    index_block.entries[0].block_size = sizeof(data_block_t);

    printf("  - Index Block 大小: %zu 字节\n", sizeof(index_block_t));
    printf("  - 索引条目: last_key='city', offset=%u, size=%u\n\n",
           index_block.entries[0].block_offset,
           index_block.entries[0].block_size);

    /* ------------------------------------------------------------------------
     * 步骤3：创建 Filter Block (Bloom Filter)
     * --------------------------------------------------------------------- */
    printf("[sstable] 步骤3：创建 Filter Block\n");
    filter_block_t filter_block;
    memset(&filter_block, 0, sizeof(filter_block));

    filter_block.header.num_entries = 1;
    filter_block.filters[0].num_hashes = 3;

    /* 将所有键添加到布隆过滤器 */
    bloom_add(&filter_block.filters[0], "name");
    bloom_add(&filter_block.filters[0], "age");
    bloom_add(&filter_block.filters[0], "city");

    printf("  - Filter Block 大小: %zu 字节\n", sizeof(filter_block_t));
    printf("  - 布隆过滤器位掩码: 0x%016llx\n", (unsigned long long)filter_block.filters[0].bitmask);
    printf("  - 已添加键: name, age, city\n\n");

    /* ------------------------------------------------------------------------
     * 步骤4：创建 Footer
     * --------------------------------------------------------------------- */
    printf("[sstable] 步骤4：创建 Footer\n");
    sstable_footer_t footer;
    memset(&footer, 0, sizeof(footer));

    footer.index_offset = BLOCK_SIZE;                           /* Index 在 Data 之后 */
    footer.index_size = sizeof(index_block_t);
    footer.filter_offset = BLOCK_SIZE + sizeof(index_block_t);  /* Filter 在 Index 之后 */
    footer.filter_size = sizeof(filter_block_t);
    footer.magic = 0x53455441424C4500ULL;                       /* "SSTABLE" */

    printf("  - Footer 大小: %zu 字节\n", sizeof(sstable_footer_t));
    printf("  - Index 偏移: %lu, 大小: %lu\n", (unsigned long)footer.index_offset, (unsigned long)footer.index_size);
    printf("  - Filter 偏移: %lu, 大小: %lu\n", (unsigned long)footer.filter_offset, (unsigned long)footer.filter_size);
    printf("  - 魔数: 0x%016lx\n\n", (unsigned long)footer.magic);

    /* ------------------------------------------------------------------------
     * 步骤5：文件布局汇总
     * --------------------------------------------------------------------- */
    printf("[sstable] 文件布局\n");
    printf("------------------\n");
    uint64_t file_size = BLOCK_SIZE + sizeof(index_block_t) + sizeof(filter_block_t) + sizeof(footer);
    printf("  [0, %d)           Data Block (%d 字节)\n", BLOCK_SIZE, BLOCK_SIZE);
    printf("  [%d, %ld)        Index Block (%ld 字节)\n",
           BLOCK_SIZE, (long)(BLOCK_SIZE + sizeof(index_block_t)), (long)sizeof(index_block_t));
    printf("  [%ld, %ld)       Filter Block (%ld 字节)\n",
           (long)(BLOCK_SIZE + sizeof(index_block_t)),
           (long)(BLOCK_SIZE + sizeof(index_block_t) + sizeof(filter_block_t)),
           (long)sizeof(filter_block_t));
    printf("  [%ld, %ld)       Footer (%ld 字节)\n",
           (long)(BLOCK_SIZE + sizeof(index_block_t) + sizeof(filter_block_t)),
           (long)file_size, (long)sizeof(footer));
    printf("  总文件大小: %ld 字节\n\n", (long)file_size);

    /* ------------------------------------------------------------------------
     * 步骤6：Bloom Filter 查询演示
     * --------------------------------------------------------------------- */
    printf("[sstable] Bloom Filter 查询演示\n");
    printf("-------------------------------\n");

    const char *test_keys[] = {"name", "gender", "age"};
    for (int i = 0; i < 3; i++) {
        bool might_exist = bloom_might_contain(&filter_block.filters[0], test_keys[i]);
        printf("  查询 '%s': %s\n", test_keys[i], might_exist ? "可能存在" : "一定不存在");
    }

    printf("\n[sstable] 演示完成\n");
    return 0;
}
