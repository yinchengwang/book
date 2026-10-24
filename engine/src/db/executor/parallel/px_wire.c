/* px_wire.c - PXB1 线格式（Gap#4 Task 8）
 *
 * 注意（controller resolutions）：
 * - CRC32 为私有表驱动实现（poly 0xEDB88320，与 rpc.h 同多项式），
 *   不链接 db_distributed，避免 executor 反向依赖 distributed。
 * - deserialize 中 vector_block_create 已预分配 null_bitmap
 *   （vector_exec.c: calloc((capacity+63)/64)），覆盖前必须先 free，否则泄漏。
 * - STRING 列部分分配失败路径统一走 free_strings()，避免只 free 数组泄漏串体。
 */
#include "db/executor/px_wire.h"
#include "db/core/columnar_store.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define PXW_MAGIC 0x31584250u   /* "PXB1" 小端 */
#define PXW_VERSION 1u

/* ---- 私有 CRC32（与 rpc.h 同多项式，避免反向依赖 db_distributed） ---- */
static uint32_t pxw_crc_table[256];
static pthread_once_t pxw_crc_once = PTHREAD_ONCE_INIT;

static void pxw_crc_table_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        pxw_crc_table[i] = c;
    }
}

static uint32_t pxw_crc32(const uint8_t *data, size_t size) {
    pthread_once(&pxw_crc_once, pxw_crc_table_init);
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++)
        crc = pxw_crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

/* 释放部分分配的字符串数组：count 个槽位（未分配槽为 NULL，free(NULL) 安全） */
static void free_strings(char **strs, int count) {
    if (!strs) return;
    for (int i = 0; i < count; i++) free(strs[i]);
    free(strs);
}

/* ---- 写游标 ---- */
typedef struct { uint8_t *buf; size_t cap, len; } wcur_t;

static int wput(wcur_t *w, const void *src, size_t n) {
    if (w->len + n > w->cap) {
        size_t ncap = w->cap ? w->cap * 2 : 4096;
        while (ncap < w->len + n) ncap *= 2;
        uint8_t *nb = (uint8_t *)realloc(w->buf, ncap);
        if (!nb) return -1;
        w->buf = nb;
        w->cap = ncap;
    }
    memcpy(w->buf + w->len, src, n);
    w->len += n;
    return 0;
}

static int wput_u32(wcur_t *w, uint32_t v) { return wput(w, &v, 4); }
static int wput_i32(wcur_t *w, int32_t v)  { return wput(w, &v, 4); }
static int wput_u8 (wcur_t *w, uint8_t v)  { return wput(w, &v, 1); }

int px_wire_serialize(const VectorBlock *b, uint32_t seq, uint32_t flags,
                      const char *err_msg, uint8_t **out, uint32_t *out_size) {
    if (!out || !out_size) return -1;
    *out = NULL;
    *out_size = 0;
    if (!b && !(flags & (PXW_FLAG_LAST | PXW_FLAG_ERR))) return -1;

    wcur_t w = {0};
    int rc = -1;
    do {
        if (wput_u32(&w, PXW_MAGIC) || wput_u8(&w, PXW_VERSION)
            || wput_u32(&w, flags) || wput_u32(&w, seq)) break;

        int32_t nrows = b ? b->num_rows : 0;
        int32_t ncols = b ? b->num_columns : 0;
        if (wput_i32(&w, nrows) || wput_i32(&w, ncols)) break;

        if (flags & PXW_FLAG_ERR) {
            uint32_t mlen = err_msg ? (uint32_t)strlen(err_msg) : 0;
            if (wput_u32(&w, mlen)) break;
            if (mlen && wput(&w, err_msg, mlen)) break;
        }

        if (b) {
            int nwords = (nrows + 63) / 64;
            for (int32_t c = 0; c < ncols; c++) {
                int32_t type = vector_block_get_column_type(b, c);
                int32_t esz = b->column_sizes[c];
                if (wput_i32(&w, type) || wput_i32(&w, esz)) goto done;
                if (type == COLUMN_STRING) {
                    const char **strs = (const char **)b->columns[c];
                    if (wput_u32(&w, (uint32_t)nrows)) goto done;
                    for (int32_t r = 0; r < nrows; r++) {
                        uint32_t slen = strs[r] ? (uint32_t)strlen(strs[r]) : 0;
                        if (wput_u32(&w, slen)) goto done;
                        if (slen && wput(&w, strs[r], slen)) goto done;
                    }
                } else {
                    if (wput(&w, b->columns[c], (size_t)esz * (size_t)nrows)) goto done;
                }
            }
            if (wput_u32(&w, (uint32_t)nwords)) goto done;
            if (nwords > 0) {
                if (b->null_bitmap) {
                    if (wput(&w, b->null_bitmap, (size_t)nwords * 8)) goto done;
                } else {
                    uint64_t zero = 0;
                    for (int i = 0; i < nwords; i++)
                        if (wput(&w, &zero, 8)) goto done;
                }
            }
        }

        uint32_t crc = pxw_crc32(w.buf, w.len);
        if (wput_u32(&w, crc)) break;
        rc = 0;
    } while (0);

done:
    if (rc == 0) {
        *out = w.buf;
        *out_size = (uint32_t)w.len;
    } else {
        free(w.buf);
    }
    return rc;
}

/* ---- 读游标 ---- */
typedef struct { const uint8_t *buf; size_t len, pos; } rcur_t;

static int rget(rcur_t *r, void *dst, size_t n) {
    if (r->pos + n > r->len) return -1;
    memcpy(dst, r->buf + r->pos, n);
    r->pos += n;
    return 0;
}

static int rget_u32(rcur_t *r, uint32_t *v) { return rget(r, v, 4); }
static int rget_i32(rcur_t *r, int32_t *v)  { return rget(r, v, 4); }
static int rget_u8 (rcur_t *r, uint8_t *v)  { return rget(r, v, 1); }

VectorBlock *px_wire_deserialize(const uint8_t *buf, uint32_t size,
                                 uint32_t *seq_out, uint32_t *flags_out) {
    if (!buf || !seq_out || !flags_out) return NULL;
    *flags_out = PXW_DESER_CRC_FAIL;
    if (size < 4 + 4) return NULL;   /* 帧长不足以含头+CRC，按解析失败处理 */

    /* CRC 校验（尾部 4 字节） */
    uint32_t stored_crc;
    memcpy(&stored_crc, buf + size - 4, 4);
    if (pxw_crc32(buf, size - 4) != stored_crc) return NULL;

    rcur_t r = {buf, size - 4, 0};
    uint32_t magic, flags, seq;
    uint8_t version;
    int32_t nrows, ncols;
    if (rget_u32(&r, &magic) || magic != PXW_MAGIC) return NULL;
    if (rget_u8(&r, &version) || version != PXW_VERSION) return NULL;
    if (rget_u32(&r, &flags) || rget_u32(&r, &seq)) return NULL;
    if (rget_i32(&r, &nrows) || rget_i32(&r, &ncols)) return NULL;

    *seq_out = seq;
    *flags_out = flags;

    if (flags & PXW_FLAG_ERR) {           /* 跳过错误消息体 */
        uint32_t mlen;
        if (rget_u32(&r, &mlen)) { *flags_out = PXW_DESER_CRC_FAIL; return NULL; }
        if ((size_t)mlen > r.len - r.pos) {   /* 越界钳制：按解析失败处理 */
            *flags_out = PXW_DESER_CRC_FAIL;
            return NULL;
        }
        r.pos += mlen;
    }

    if (nrows <= 0 || ncols <= 0) return NULL;   /* 纯标记帧 */

    VectorBlock *b = vector_block_create(nrows, ncols);
    if (!b) { *flags_out = PXW_DESER_CRC_FAIL; return NULL; }

    for (int32_t c = 0; c < ncols; c++) {
        int32_t type, esz;
        if (rget_i32(&r, &type) || rget_i32(&r, &esz)) goto fail;
        vector_block_set_column_type(b, c, type);
        if (type == COLUMN_STRING) {
            uint32_t nstr;
            if (rget_u32(&r, &nstr) || nstr != (uint32_t)nrows) goto fail;
            char **strs = (char **)calloc((size_t)nrows, sizeof(char *));
            if (!strs) goto fail;
            for (int32_t i = 0; i < nrows; i++) {
                uint32_t slen;
                /* calloc 清零，未分配槽为 NULL，free_strings 对任意前缀安全 */
                if (rget_u32(&r, &slen)) { free_strings(strs, nrows); goto fail; }
                strs[i] = (char *)malloc(slen + 1);
                if (!strs[i]) { free_strings(strs, nrows); goto fail; }
                if (rget(&r, strs[i], slen)) { free_strings(strs, nrows); goto fail; }
                strs[i][slen] = '\0';
            }
            vector_block_set_column(b, c, strs, sizeof(char *));
        } else {
            void *col = malloc((size_t)esz * (size_t)nrows);
            if (!col) goto fail;
            if (rget(&r, col, (size_t)esz * (size_t)nrows)) { free(col); goto fail; }
            vector_block_set_column(b, c, col, esz);
        }
    }

    {
        uint32_t nwords;
        if (rget_u32(&r, &nwords)) goto fail;
        if (nwords > 0) {
            /* vector_block_create 已预分配 null_bitmap，直接覆盖会泄漏——先释放 */
            free(b->null_bitmap);
            b->null_bitmap = (uint64_t *)calloc(nwords, 8);
            if (!b->null_bitmap) goto fail;
            if (rget(&r, b->null_bitmap, (size_t)nwords * 8)) goto fail;
        }
    }
    vector_block_set_num_rows(b, nrows);
    return b;

fail:
    /* vector_block_destroy 只 free 列数组本身，不 free STRING 列内的串体；
       已挂到 block 上的 STRING 列在这里先逐串释放，避免失败路径泄漏。 */
    for (int32_t c = 0; c < ncols; c++) {
        if (vector_block_get_column_type(b, c) == COLUMN_STRING && b->columns[c]) {
            free_strings((char **)b->columns[c], nrows);
            b->columns[c] = NULL;   /* 数组已随 free_strings 释放，防 destroy 二次释放 */
        }
    }
    vector_block_destroy(b);
    *flags_out = PXW_DESER_CRC_FAIL;
    return NULL;
}

/* A1（task-8 复审 I-2）：vector_block_destroy 只 free 列数组本体，
 * 不 free STRING 列内逐行堆分配的串载荷；本深销毁先逐串 free，
 * 串数组本体仍交给 vector_block_destroy 统一释放。 */
void px_wire_block_destroy(VectorBlock *b) {
    if (!b) return;
    for (int32_t c = 0; c < b->num_columns; c++) {
        if (vector_block_get_column_type(b, c) == COLUMN_STRING && b->columns[c]) {
            char **strs = (char **)b->columns[c];
            for (int i = 0; i < b->num_rows; i++) free(strs[i]);
        }
    }
    vector_block_destroy(b);
}
