/**
 * @file px_wire.h
 * @brief Gap#4 pxwire——VectorBlock 线格式（PXB1）
 *
 * 布局：头（magic/version/flags/seq/rows/cols）+ 每列（type/elem_size/data）
 * + 行级 null 位图 + CRC32 尾。小端。
 * 定长列（INT32/INT64/FLOAT/DOUBLE）：data = elem_size * num_rows。
 * 字符串列（COLUMN_STRING，char**）：u32 串数 + 每串 u32 len + 原始字节。
 * 纯标记帧（b==NULL）：只含头+CRC，用于 LAST/ERR。
 * 错误帧（flags&PXW_FLAG_ERR）：err_msg 以 u32 len + bytes 附于头后。
 * 反序列化返回的 VectorBlock 其 STRING 列串载荷为堆分配；
 * vector_block_destroy 不释放串载荷，接收方销毁前须自行逐串 free。
 */
#ifndef DB_EXECUTOR_PX_WIRE_H
#define DB_EXECUTOR_PX_WIRE_H

#include <stdint.h>
#include "db/core/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PXW_FLAG_LAST 0x1u
#define PXW_FLAG_ERR  0x2u
#define PXW_DESER_CRC_FAIL 0xFFFFFFFFu

int px_wire_serialize(const VectorBlock *b, uint32_t seq, uint32_t flags,
                      const char *err_msg, uint8_t **out, uint32_t *out_size);
VectorBlock *px_wire_deserialize(const uint8_t *buf, uint32_t size,
                                 uint32_t *seq_out, uint32_t *flags_out);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_PX_WIRE_H */
