/**
 * @file mm_record.h
 * @brief mm_insert/mm_get 序列化契约（C0-3 T6）
 *
 * 所有模态写入 mm_insert 的数据统一前缀 mm_record_header_t，读取端据此
 * 判定数据版本与模态类型。旧格式（无头部）通过 magic 不匹配探测，
 * 回退历史字节偏移解析路径以保持向后兼容。
 */
#ifndef DB_MM_RECORD_H
#define DB_MM_RECORD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 头部 magic：'MMDB' = 0x4D4D4442（little-endian: 0x42444D4D） */
#define MM_RECORD_MAGIC       0x42444D4Du  /* 'BDMM' LE = 'MMDB' BE */
#define MM_RECORD_VERSION_V1  1u

/**
 * @brief mm_insert 数据统一头部（16 字节定长前缀）
 *
 * 字段顺序固定，禁止调整（ABI 稳定）：
 *   magic:      用于探测是否为新格式（old data 无此字段时 magic 不匹配）
 *   version:    当前 1；未来 2+ 字段含义见各自实现
 *   model:      DataModel 枚举值（0..9，详见 storage_engine.h）
 *   payload_len:紧随头部后的负载字节数
 */
typedef struct mm_record_header_s {
    uint32_t magic;          /* 应为 MM_RECORD_MAGIC */
    uint32_t version;        /* 应为 MM_RECORD_VERSION_V1 */
    uint32_t model;          /* DataModel 枚举 */
    uint32_t payload_len;    /* 负载长度 */
} mm_record_header_t;

/* 编译期 sanity check */
typedef char mm_record_header_size_check[sizeof(mm_record_header_t) == 16 ? 1 : -1];

/**
 * @brief 检测数据是否带新格式头部（magic 探测）
 * @param data 用户数据指针
 * @param len  数据总长度
 * @return true 数据以 mm_record_header_t 开头且 magic 匹配；false 旧格式
 */
int mm_record_has_header(const void *data, size_t len);

/**
 * @brief 写入头部到 buf（辅助 mm_insert 路径）
 * @param buf 输出缓冲（至少 sizeof(mm_record_header_t) + payload_len）
 * @param model DataModel 枚举
 * @param payload_len 负载长度
 * @return 头部字节数（16）
 */
size_t mm_record_write_header(void *buf, uint32_t model, uint32_t payload_len);

/* C3-1 T10：mm_storage 接入 BLOB 类型 */
#define MM_BLOB_MAX_REF_LEN 64
int mm_storage_blob_put(const char *collection, const uint8_t blob_id[32]);
int mm_storage_blob_get(const char *collection,
                        const uint8_t blob_id[32],
                        void *out_buf, size_t buf_len, size_t *out_read);

#ifdef __cplusplus
}
#endif

#endif /* DB_MM_RECORD_H */
