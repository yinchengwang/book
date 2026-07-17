/**
 * @file heap_vector_store.h
 * @brief 向量 Heap 主存储（向量的 Single Source of Truth）
 *
 * 设计目标
 * --------
 * - Heap 是多模态索引子系统中**唯一持有完整向量**的组件，其它索引
 *   （HNSW、IVF、DiskANN 等）只保存 `vector_ref_t`，运行时通过本接口
 *   按需还原真实向量。
 * - 页面分配 / 释放 / 持久化均委托给 `storage_backend_t`，因此上层
 *   可以自由选择内存、页面文件、mmap 等不同的物理介质。
 *
 * 页面布局
 * --------
 * 每个 Heap 页固定为配置时声明的 `page_size`（默认 8KB），布局如下：
 *
 *   +---------------------+-------------------+--------------------+
 *   |  magic (uint32)     |  slot_count       |  vector_0          |
 *   |  (4 字节)           |  (int32, 4 字节)  |  (dims * 4 字节)   |
 *   +---------------------+-------------------+--------------------+
 *   |  vector_1           |  vector_2         |  ...               |
 *   |  (dims * 4 字节)    |  (dims * 4 字节)  |                    |
 *   +---------------------+-------------------+--------------------+
 *
 * - 头 8 字节是页面元数据：`magic` 标记为合法 Heap 页（`HEAP_PAGE_MAGIC`），
 *   `slot_count` 记录当前已写入的向量槽数（最大 `vectors_per_page`）。
 * - 向量采用定长槽位（连续 float 数组），保证 O(1) 偏移定位。
 * - `page_id == INVALID_PAGE_ID` 表示"未分配"，写入时按需向存储后端申请。
 *
 * 与 vector_ref 的协作
 * --------------------
 * `heap_vector_insert` 返回的 `vector_ref_t` 满足：
 *
 *   ref.heap_page_id = 页 ID（>= 0）
 *   ref.offset       = 数据在页内的起始偏移（= PAGE_HEADER_SIZE + idx * vector_bytes）
 *   ref.length       = dims * sizeof(float)
 *
 * `vector_ref_is_valid` 可直接用于完整性检查。
 *
 * 所有权与生命周期
 * ----------------
 *
 * heap_vector_store_t 所有权规则：
 *
 * 1. 调用方创建并拥有 heap_store，负责调用 heap_vector_store_destroy()
 * 2. 索引通过 faiss_hnsw_index_set_heap_store() 绑定后，索引**不接管**所有权
 * 3. 索引 drop 时仅置空 heap_store 指针，不销毁 heap_store
 * 4. 调用方必须在索引销毁后自行管理 heap_store 生命周期
 *
 * 典型使用模式：
 * - 外部创建 heap_store 并绑定索引：索引 drop 后，调用方仍需调用 heap_vector_store_destroy()
 * - 推荐在索引生命周期外创建/销毁 heap_store，避免悬空指针
 *
 * 补充说明：
 * - `heap_vector_store_create` 不接管 `backend` 所有权；
 *   `heap_vector_store_destroy` 仅释放自身结构，不调用 `ops->close`。
 * - 多线程场景：当前实现假定单一写入者 / 多读取者；并发写需自行加锁。
 */
#ifndef DB_HEAP_VECTOR_STORE_H
#define DB_HEAP_VECTOR_STORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "db/index/vector_ref.h"
#include "db/index/storage_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 默认页面大小（8KB，与 PostgreSQL 默认一致） */
#define HEAP_VECTOR_DEFAULT_PAGE_SIZE 8192u

/** 页面头部大小（magic 4B + slot_count 4B） */
#define HEAP_VECTOR_PAGE_HEADER_SIZE 8u

/** Heap 页 magic 标识（"HVEC"），用于校验页面合法性 */
#define HEAP_VECTOR_PAGE_MAGIC 0x48464543u

/* ============================================================
 * 类型定义
 * ============================================================ */

/** Heap 向量存储（不透明指针） */
typedef struct heap_vector_store heap_vector_store_t;

/**
 * @brief Heap 向量存储配置
 *
 * 字段说明：
 * - backend：存储后端，必须非空且 ops 函数表完整（至少提供
 *            alloc_page / read_page / write_page 三个回调）。
 * - dims：向量维度，必须 > 0。
 * - page_size：页面大小，0 表示使用默认值 `HEAP_VECTOR_DEFAULT_PAGE_SIZE`。
 *              若非 0，必须与 `backend->page_size` 保持一致（避免越界 IO）。
 * - vectors_per_page：每页可容纳的向量数，0 表示由实现根据 dims 和
 *                     page_size 自动计算。
 */
typedef struct heap_vector_config {
    storage_backend_t *backend;        /**< 存储后端（必须非空） */
    int32_t            dims;           /**< 向量维度（> 0） */
    size_t             page_size;      /**< 页面大小，0 走默认 */
    int                vectors_per_page; /**< 0 表示自动计算 */
} heap_vector_config_t;

/* ============================================================
 * 公共 API
 * ============================================================ */

/**
 * @brief 创建 Heap 向量存储
 *
 * @param config 存储配置（必须非空）
 * @return 新创建的 Heap；失败返回 NULL
 *
 * @note 不接管 `config->backend` 所有权；调用方需自行管理后端生命周期。
 */
heap_vector_store_t *heap_vector_store_create(const heap_vector_config_t *config);

/**
 * @brief 销毁 Heap 向量存储
 *
 * 仅释放 Heap 自身结构，不调用 `backend->ops->close`。
 * @param store Heap 指针（允许 NULL）
 */
void heap_vector_store_destroy(heap_vector_store_t *store);

/**
 * @brief 插入单个向量
 *
 * 自动按 `vectors_per_page` 切页，写入失败或参数非法时返回无效引用。
 *
 * @param store  Heap 存储（必须非空）
 * @param vector 待写入向量，长度必须为 `store->dims` 浮点元素
 * @return 写入成功则返回有效 `vector_ref_t`；失败返回 `vector_ref_make_invalid()`
 */
vector_ref_t heap_vector_insert(heap_vector_store_t *store, const float *vector);

/**
 * @brief 批量插入向量
 *
 * 任一元素插入失败将立即中止并返回 -1，已成功写入的引用会保留在
 * `refs_out` 中（caller 仍可通过 `vector_ref_is_valid` 区分）。
 *
 * @param store     Heap 存储
 * @param vectors   连续向量缓冲，长度 `count * dims`
 * @param count     待写入向量数
 * @param refs_out  输出引用数组，长度 `count`
 * @return 0 表示全部成功；非 0 表示失败
 */
int heap_vector_insert_batch(heap_vector_store_t *store,
                             const float *vectors,
                             int32_t count,
                             vector_ref_t *refs_out);

/**
 * @brief 通过引用读取向量
 *
 * 严格校验 `ref` 的偏移和长度，若越出对应页的有效槽位则返回 -1。
 *
 * @param store     Heap 存储
 * @param ref       向量引用
 * @param out_vector  输出缓冲，长度至少为 `store->dims`
 * @return 0 成功；非 0 失败
 */
int heap_vector_get(const heap_vector_store_t *store,
                    const vector_ref_t *ref,
                    float *out_vector);

/**
 * @brief 批量读取向量
 *
 * @param store         Heap 存储
 * @param refs          引用数组，长度 `count`
 * @param count         引用数
 * @param out_vectors   输出缓冲，长度 `count * dims`
 * @return 0 成功；非 0 表示首个失败位置已停止
 */
int heap_vector_get_batch(const heap_vector_store_t *store,
                          const vector_ref_t *refs,
                          int32_t count,
                          float *out_vectors);

/**
 * @brief 获取已插入向量总数
 *
 * @param store Heap 存储
 * @return 已插入向量数；store 为 NULL 时返回 0
 */
int32_t heap_vector_count(const heap_vector_store_t *store);

/**
 * @brief 获取每页可容纳的向量数
 *
 * @param store Heap 存储
 * @return 每页槽位数；store 为 NULL 时返回 0
 */
int heap_vector_capacity_per_page(const heap_vector_store_t *store);

/**
 * @brief 获取向量维度
 *
 * @param store Heap 存储
 * @return 维度；store 为 NULL 时返回 0
 */
int32_t heap_vector_dims(const heap_vector_store_t *store);

#ifdef __cplusplus
}
#endif

#endif /* DB_HEAP_VECTOR_STORE_H */
