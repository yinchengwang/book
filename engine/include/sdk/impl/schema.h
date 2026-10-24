/**
 * @file schema.h
 * @brief Schema 验证与序列化（内部接口）
 */
#ifndef SDK_IMPL_SCHEMA_H
#define SDK_IMPL_SCHEMA_H

#include "sdk/mmdb.h"
#include "sdk/impl/mmdb_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 验证 schema 与 model 是否匹配；返回 MMDB_OK 或错误码 */
int mmdb_schema_validate(const mmdb_schema_t* schema);

/* 将 schema 序列化为紧凑 JSON（返回堆字符串，调用者 free） */
char* mmdb_schema_to_json(const mmdb_schema_t* schema);

/* 从 JSON 反序列化为 schema（成功填充 out_schema，调用者释放 fields） */
int mmdb_schema_from_json(const char* json, mmdb_schema_t* out_schema);

#ifdef __cplusplus
}
#endif

#endif /* SDK_IMPL_SCHEMA_H */