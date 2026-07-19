/**
 * @file page.h
 * @brief 页面管理 - 数据库存储的基本单元
 *
 * 页面（Page）是数据库存储的基本单位，默认大小为 8KB
 */
#ifndef DB_PAGE_H
#define DB_PAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 默认页面大小：64KB（用于存储较大的图结构体） */
#define DEFAULT_PAGE_SIZE 65536

/** 页面头部大小：16 字节 */
#define PAGE_HEADER_SIZE 16

/** 页面数据区大小 */
#define PAGE_DATA_SIZE (DEFAULT_PAGE_SIZE - PAGE_HEADER_SIZE)

/* ============================================================
 * 页面类型枚举
 * ============================================================ */

/** 页面类型 */
typedef enum page_type_e {
    PAGE_FREE = 0,      /**< 空闲页 */
    PAGE_DATA = 1,     /**< 数据页 */
    PAGE_INDEX = 2,    /**< 索引页 */
    PAGE_OVERFLOW = 3, /**< 溢出页（大字段） */
    PAGE_META = 4      /**< 元数据页 */
} page_type_t;

/* ============================================================
 * 页面结构
 * ============================================================ */

/**
 * @brief 页面头部结构（紧凑排列，16 字节）
 */
#pragma pack(push, 1)
typedef struct page_header_s {
    uint32_t page_id;           /**< 页面 ID */
    uint8_t  page_type;         /**< 页面类型 */
    uint16_t checksum;          /**< 校验和 */
    uint16_t free_space_offset; /**< 空闲空间起始偏移 */
    uint8_t  reserved[7];       /**< 保留字段 */
} page_header_t;
#pragma pack(pop)

/**
 * @brief 页面结构
 *
 * 页面由头部和数据区组成：
 * - 头部：16 字节，包含页面元信息
 * - 数据区：8176 字节，存储实际数据
 */
typedef struct page_s {
    page_header_t header;  /**< 页面头部 */
    uint8_t data[PAGE_DATA_SIZE];  /**< 页面数据区 */
} page_t;

/* ============================================================
 * 页面操作函数
 * ============================================================ */

/**
 * @brief 创建新页面
 * @param page_id 页面 ID
 * @param type 页面类型
 * @return 新创建的页面指针
 */
page_t *page_create(uint32_t page_id, page_type_t type);

/**
 * @brief 释放页面
 * @param page 页面指针
 */
void page_free(page_t *page);

/**
 * @brief 获取页面可用空间大小
 * @param page 页面指针
 * @return 可用空间字节数
 */
size_t page_get_free_space(const page_t *page);

/**
 * @brief 获取页面已用空间大小
 * @param page 页面指针
 * @return 已用空间字节数
 */
size_t page_get_used_space(const page_t *page);

/**
 * @brief 在页面中分配空间
 * @param page 页面指针
 * @param size 需要分配的大小
 * @return 分配的空间起始偏移，失败返回 (uint16_t)-1
 */
uint16_t page_alloc_space(page_t *page, size_t size);

/**
 * @brief 设置页面校验和
 * @param page 页面指针
 */
void page_set_checksum(page_t *page);

/**
 * @brief 验证页面校验和
 * @param page 页面指针
 * @return 校验通过返回 true
 */
bool page_verify_checksum(const page_t *page);

/**
 * @brief 计算指定大小字节数组的校验和
 * @param bytes 字节数组
 * @param page_size 页面大小（字节数）
 * @return 校验和值
 *
 * 用于 Buffer Pool 场景：Bufread 使用 BUF_PAGE_SIZE (8192) 而非 sizeof(page_t) (65536)
 */
uint16_t page_calc_checksum_bytes(const uint8_t *bytes, size_t page_size);

/**
 * @brief 获取页面类型名称
 * @param type 页面类型
 * @return 类型名称字符串
 */
const char *page_type_name(page_type_t type);

/* ============================================================
 * 页面数据读写宏
 * ============================================================ */

/**
 * @brief 写入数据到页面
 * @param page 页面指针
 * @param offset 偏移
 * @param data 数据指针
 * @param size 数据大小
 */
static inline void page_write_data(page_t *page, uint16_t offset,
                                   const void *data, size_t size) {
    if (offset + size <= PAGE_DATA_SIZE) {
        memcpy(page->data + offset, data, size);
    }
}

/**
 * @brief 从页面读取数据
 * @param page 页面指针
 * @param offset 偏移
 * @param data 数据指针（输出）
 * @param size 数据大小
 */
static inline void page_read_data(const page_t *page, uint16_t offset,
                                  void *data, size_t size) {
    if (offset + size <= PAGE_DATA_SIZE) {
        memcpy(data, page->data + offset, size);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* DB_PAGE_H */
