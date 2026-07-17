/**
 * @file heap_vector_store.c
 * @brief 向量 Heap 主存储实现
 *
 * 通过 storage_backend_t 完成页面分配、读写与刷盘，自身只维护
 * 元数据：维度、页面大小、每页槽数、累计插入量。
 *
 * 并发模型：单写者 / 多读者。所有公开 API 内部不持有全局锁，
 * 由调用方根据使用场景自行同步。
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "db/index/heap/heap_vector_store.h"

/* ============================================================
 * 内部结构
 * ============================================================ */

/** Heap 向量存储上下文 */
struct heap_vector_store {
    storage_backend_t *backend;        /**< 存储后端（不持有所有权） */
    int32_t            dims;           /**< 向量维度 */
    size_t             page_size;      /**< 页面大小（字节） */
    int                vectors_per_page; /**< 每页可容纳向量数 */
    int32_t            total_vectors;  /**< 已成功插入的向量总数 */
};

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 计算每页可容纳的向量数
 *
 * @param dims      向量维度
 * @param page_size 页面大小
 * @return 每页槽位数；参数非法返回 0
 */
static int calc_vectors_per_page(int32_t dims, size_t page_size)
{
    if (dims <= 0 || page_size <= HEAP_VECTOR_PAGE_HEADER_SIZE) {
        return 0;
    }
    size_t vector_bytes = (size_t)dims * sizeof(float);
    if (vector_bytes == 0) {
        return 0;
    }
    size_t available = page_size - HEAP_VECTOR_PAGE_HEADER_SIZE;
    size_t n = available / vector_bytes;
    if (n == 0 || n > (size_t)INT32_MAX) {
        return 0;
    }
    return (int)n;
}

/**
 * @brief 校验后端及其 ops 是否满足 Heap 的最低要求
 */
static bool backend_is_valid(const storage_backend_t *backend)
{
    if (backend == NULL || backend->ops == NULL || backend->ctx == NULL) {
        return false;
    }
    if (backend->ops->alloc_page == NULL
        || backend->ops->read_page == NULL
        || backend->ops->write_page == NULL) {
        return false;
    }
    if (backend->page_size < HEAP_VECTOR_PAGE_HEADER_SIZE + sizeof(float)) {
        return false;
    }
    return true;
}

/**
 * @brief 计算当前要写入的"页号"和"页内槽位"
 *
 * @param store            Heap 存储
 * @param out_page_id      输出页号（写入后端时可能因为 alloc_page 调整）
 * @param out_slot_index   输出槽位（0 <= slot < vectors_per_page）
 * @return true 计算成功；false 表示后端被填满（int32 上溢）
 */
static bool plan_next_slot(const heap_vector_store_t *store,
                           page_id_t *out_page_id,
                           int *out_slot_index)
{
    int32_t total = store->total_vectors;
    int vpp = store->vectors_per_page;
    if (vpp <= 0) {
        return false;
    }
    int32_t page_index = total / vpp;
    int32_t slot_index = total % vpp;
    if (page_index > (int32_t)INT32_MAX) {
        return false;
    }
    *out_page_id = (page_id_t)page_index;
    *out_slot_index = (int)slot_index;
    return true;
}

/**
 * @brief 解析 Heap 页面头部元数据
 *
 * @param page          页面缓冲（至少 page_size 字节）
 * @param page_size     页面大小
 * @param out_slot_count 输出当前页已写入的槽数
 * @return true 头部合法；false 表示 magic 错配或数据截断
 */
static bool read_page_header(const void *page,
                             size_t page_size,
                             int32_t *out_slot_count)
{
    if (page == NULL || page_size < HEAP_VECTOR_PAGE_HEADER_SIZE) {
        return false;
    }
    uint32_t magic;
    int32_t  slot_count;
    memcpy(&magic, page, sizeof(magic));
    memcpy(&slot_count, (const char *)page + sizeof(uint32_t), sizeof(slot_count));
    if (magic != HEAP_VECTOR_PAGE_MAGIC) {
        return false;
    }
    if (slot_count < 0) {
        return false;
    }
    *out_slot_count = slot_count;
    return true;
}

/**
 * @brief 写入 Heap 页面头部
 */
static void write_page_header(void *page, int32_t slot_count)
{
    uint32_t magic = HEAP_VECTOR_PAGE_MAGIC;
    memcpy(page, &magic, sizeof(magic));
    memcpy((char *)page + sizeof(uint32_t), &slot_count, sizeof(slot_count));
}

/* ============================================================
 * 公共 API 实现
 * ============================================================ */

heap_vector_store_t *heap_vector_store_create(const heap_vector_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }
    if (!backend_is_valid(config->backend)) {
        return NULL;
    }
    if (config->dims <= 0) {
        return NULL;
    }

    /* 解析页面大小：0 -> 默认；非 0 必须与后端保持一致 */
    size_t page_size = config->page_size;
    if (page_size == 0) {
        page_size = HEAP_VECTOR_DEFAULT_PAGE_SIZE;
    }
    if (page_size != config->backend->page_size) {
        return NULL;
    }
    if (page_size > (size_t)INT32_MAX) {
        return NULL;
    }

    /* 计算每页槽数 */
    int vpp = config->vectors_per_page > 0
              ? config->vectors_per_page
              : calc_vectors_per_page(config->dims, page_size);
    if (vpp <= 0) {
        return NULL;
    }

    heap_vector_store_t *store = (heap_vector_store_t *)calloc(1, sizeof(*store));
    if (store == NULL) {
        return NULL;
    }
    store->backend          = config->backend;
    store->dims             = config->dims;
    store->page_size        = page_size;
    store->vectors_per_page = vpp;
    store->total_vectors    = 0;
    return store;
}

void heap_vector_store_destroy(heap_vector_store_t *store)
{
    if (store == NULL) {
        return;
    }
    /* 不接管 backend 所有权，仅释放自身结构 */
    free(store);
}

vector_ref_t heap_vector_insert(heap_vector_store_t *store, const float *vector)
{
    vector_ref_t ref = vector_ref_make_invalid();
    if (store == NULL || vector == NULL) {
        return ref;
    }

    page_id_t page_id = INVALID_PAGE_ID;
    int slot_index = 0;
    if (!plan_next_slot(store, &page_id, &slot_index)) {
        return ref;
    }

    size_t vector_bytes = (size_t)store->dims * sizeof(float);
    size_t data_offset  = HEAP_VECTOR_PAGE_HEADER_SIZE + (size_t)slot_index * vector_bytes;

    /* 申请 / 复用页面：第一个向量必须显式分配；之后每跨页时分配新页 */
    if (store->total_vectors == 0 || slot_index == 0) {
        page_id = store->backend->ops->alloc_page(store->backend->ctx);
        if (page_id == INVALID_PAGE_ID) {
            return ref;
        }
    }

    /* 申请临时页面缓冲（按 page_size 动态分配，避免大页面入栈） */
    void *page = malloc(store->page_size);
    if (page == NULL) {
        return ref;
    }
    memset(page, 0, store->page_size);

    /* 新分配的页面在持久化后端可能已经存在旧数据（mtime），先 read 一遍 */
    if (store->backend->ops->read_page(store->backend->ctx, page_id, page) == 0) {
        int32_t existing_slots = 0;
        if (!read_page_header(page, store->page_size, &existing_slots)) {
            /* 旧页或空页：按全零初始化，slot_count=0 */
            memset(page, 0, store->page_size);
        }
    }
    /* 若是第一次写入（已分配新页且 read 失败），page 已是零缓冲 */

    /* 写入向量数据 */
    memcpy((char *)page + data_offset, vector, vector_bytes);

    /* 更新头部 slot_count = slot_index + 1 */
    int32_t new_slot_count = slot_index + 1;
    write_page_header(page, new_slot_count);

    if (store->backend->ops->write_page(store->backend->ctx, page_id, page) != 0) {
        free(page);
        return ref;
    }

    free(page);

    ref.heap_page_id = page_id;
    ref.offset       = (uint32_t)data_offset;
    ref.length       = (uint32_t)vector_bytes;
    store->total_vectors++;
    return ref;
}

int heap_vector_insert_batch(heap_vector_store_t *store,
                             const float *vectors,
                             int32_t count,
                             vector_ref_t *refs_out)
{
    if (store == NULL || vectors == NULL || refs_out == NULL) {
        return -1;
    }
    if (count < 0) {
        return -1;
    }
    for (int32_t i = 0; i < count; i++) {
        const float *vec = vectors + (size_t)i * (size_t)store->dims;
        refs_out[i] = heap_vector_insert(store, vec);
        if (!vector_ref_is_valid(&refs_out[i])) {
            return -1;
        }
    }
    return 0;
}

int heap_vector_get(const heap_vector_store_t *store,
                    const vector_ref_t *ref,
                    float *out_vector)
{
    if (store == NULL || ref == NULL || out_vector == NULL) {
        return -1;
    }
    if (!vector_ref_is_valid(ref)) {
        return -1;
    }

    size_t vector_bytes = (size_t)store->dims * sizeof(float);
    if (ref->length != vector_bytes) {
        /* 长度必须与 store 维度严格匹配，避免越界复制 */
        return -1;
    }
    size_t data_offset = ref->offset;
    if (data_offset < HEAP_VECTOR_PAGE_HEADER_SIZE
        || data_offset + vector_bytes > store->page_size) {
        return -1;
    }
    if (((data_offset - HEAP_VECTOR_PAGE_HEADER_SIZE) % vector_bytes) != 0) {
        /* 偏移必须落在某个槽位起点 */
        return -1;
    }

    void *page = malloc(store->page_size);
    if (page == NULL) {
        return -1;
    }
    if (store->backend->ops->read_page(store->backend->ctx, ref->heap_page_id, page) != 0) {
        free(page);
        return -1;
    }

    /* 校验页面头：槽位索引必须小于 slot_count */
    int32_t slot_count = 0;
    if (!read_page_header(page, store->page_size, &slot_count)) {
        free(page);
        return -1;
    }
    size_t slot_index = (data_offset - HEAP_VECTOR_PAGE_HEADER_SIZE) / vector_bytes;
    if ((int32_t)slot_index >= slot_count) {
        free(page);
        return -1;
    }

    memcpy(out_vector, (char *)page + data_offset, vector_bytes);
    free(page);
    return 0;
}

int heap_vector_get_batch(const heap_vector_store_t *store,
                          const vector_ref_t *refs,
                          int32_t count,
                          float *out_vectors)
{
    if (store == NULL || refs == NULL || out_vectors == NULL) {
        return -1;
    }
    if (count < 0) {
        return -1;
    }
    for (int32_t i = 0; i < count; i++) {
        const float *out = out_vectors + (size_t)i * (size_t)store->dims;
        if (heap_vector_get(store, &refs[i], (float *)out) != 0) {
            return -1;
        }
    }
    return 0;
}

int32_t heap_vector_count(const heap_vector_store_t *store)
{
    return store ? store->total_vectors : 0;
}

int heap_vector_capacity_per_page(const heap_vector_store_t *store)
{
    return store ? store->vectors_per_page : 0;
}

int32_t heap_vector_dims(const heap_vector_store_t *store)
{
    return store ? store->dims : 0;
}
