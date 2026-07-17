#ifndef HNSW_PAGE_H
#define HNSW_PAGE_H

#include <stdint.h>

#define HNSW_PAGE_SIZE 8192
#define HNSW_MAGIC_NUMBER 0xA953A953
#define HNSW_PAGE_ID 0xFF90

// 元页面布局
typedef struct {
    uint32_t magic;
    uint32_t version;
    int32_t dims;
    int32_t M;
    int32_t ef_construction;
    int32_t ef_search;
    int32_t max_level;
    int32_t entry_point;
    int32_t n_total;
} hnsw_meta_page_t;

// 页面头部
typedef struct {
    uint16_t page_id;
    uint16_t unused;
    uint32_t next_blkno;
} hnsw_page_header_t;

#ifdef __cplusplus
extern "C" {
#endif

int32_t hnsw_page_init(void *page, uint32_t blkno);
int32_t hnsw_page_read(const char *path, uint32_t blkno, void *page);
int32_t hnsw_page_write(const char *path, uint32_t blkno, const void *page);

#ifdef __cplusplus
}
#endif
#endif
