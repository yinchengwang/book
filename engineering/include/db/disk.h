/**
 * @file disk.h
 * @brief 磁盘文件操作 - 底层文件系统交互
 *
 * 负责数据库文件的创建、读写、扩展等底层操作
 */
#ifndef DB_DISK_H
#define DB_DISK_H

#include "db/page.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** 页面 ID 类型 */
typedef uint32_t page_id_t;

/** 无效的页面 ID */
#define INVALID_PAGE_ID ((page_id_t)-1)

/** 数据库文件句柄 */
typedef struct db_file_s db_file_t;

/* ============================================================
 * 文件操作函数
 * ============================================================ */

/**
 * @brief 打开或创建数据库文件
 * @param path 数据库路径
 * @param page_size 页面大小
 * @return 文件句柄，失败返回 NULL
 */
db_file_t *disk_open(const char *path, size_t page_size);

/**
 * @brief 关闭数据库文件
 * @param file 文件句柄
 */
void disk_close(db_file_t *file);

/**
 * @brief 检查数据库文件是否存在
 * @param path 数据库路径
 * @return 存在返回 true
 */
bool disk_exists(const char *path);

/**
 * @brief 获取文件中的页面数量
 * @param file 文件句柄
 * @return 页面数量
 */
page_id_t disk_get_page_count(const db_file_t *file);

/**
 * @brief 获取页面大小
 * @param file 文件句柄
 * @return 页面大小（字节）
 */
size_t disk_get_page_size(const db_file_t *file);

/**
 * @brief 读取指定页面
 * @param file 文件句柄
 * @param page_id 页面 ID
 * @return 页面指针，失败返回 NULL
 */
page_t *disk_read_page(db_file_t *file, page_id_t page_id);

/**
 * @brief 写入指定页面
 * @param file 文件句柄
 * @param page_id 页面 ID
 * @param page 页面指针
 * @return 成功返回 0
 */
int disk_write_page(db_file_t *file, page_id_t page_id, const page_t *page);

/**
 * @brief 分配新页面
 * @param file 文件句柄
 * @param type 页面类型
 * @param out_page_id 输出：新页面 ID
 * @return 新页面指针
 */
page_t *disk_alloc_page(db_file_t *file, page_type_t type, page_id_t *out_page_id);

/**
 * @brief 释放页面（标记为空闲）
 * @param file 文件句柄
 * @param page_id 页面 ID
 * @return 成功返回 0
 */
int disk_free_page(db_file_t *file, page_id_t page_id);

/**
 * @brief 同步（刷盘）
 * @param file 文件句柄
 * @return 成功返回 0
 */
int disk_sync(db_file_t *file);

/**
 * @brief 获取文件路径
 * @param file 文件句柄
 * @return 文件路径
 */
const char *disk_get_path(const db_file_t *file);

/**
 * @brief 获取文件大小
 * @param file 文件句柄
 * @return 文件大小（字节）
 */
uint64_t disk_get_size(db_file_t *file);

/**
 * @brief 读取文件指定偏移的数据
 * @param file 文件句柄
 * @param offset 文件偏移
 * @param buf 输出缓冲区
 * @param count 要读取的字节数
 * @return 实际读取的字节数，失败返回 -1
 */
ssize_t disk_pread(db_file_t *file, uint64_t offset, void *buf, size_t count);

/**
 * @brief 写入数据到文件指定偏移
 * @param file 文件句柄
 * @param offset 文件偏移
 * @param buf 数据缓冲区
 * @param count 要写入的字节数
 * @return 实际写入的字节数，失败返回 -1
 */
ssize_t disk_pwrite(db_file_t *file, uint64_t offset, const void *buf, size_t count);

/**
 * @brief 直接打开文件（不初始化头部，用于 WAL 等特殊用途）
 * @param path 文件路径
 * @return 文件句柄，失败返回 NULL
 */
db_file_t *disk_open_raw(const char *path);

/* ============================================================
 * 元数据操作
 * ============================================================ */

/**
 * @brief 读取元数据
 * @param file 文件句柄
 * @param key 元数据键
 * @param value 输出缓冲区
 * @param len 输入：缓冲区大小，输出：实际长度
 * @return 成功返回 0，不存在返回 1
 */
int disk_get_meta(db_file_t *file, const char *key, void *value, size_t *len);

/**
 * @brief 设置元数据
 * @param file 文件句柄
 * @param key 元数据键
 * @param value 元数据值
 * @param len 值长度
 * @return 成功返回 0
 */
int disk_set_meta(db_file_t *file, const char *key, const void *value, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* DB_DISK_H */
