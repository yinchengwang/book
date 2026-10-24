/**
 * @file test_data_helper.h
 * @brief 数据插入辅助函数头文件
 *
 * 提供测试中插入数据到表的辅助函数
 */
#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t Oid;

/**
 * @brief 插入测试元组到表
 * @param table_oid 表 OID
 * @param id ID 列值
 * @param name 名称列值
 * @param value 数值列值
 * @return 0 成功，-1 失败
 */
int insert_test_tuple(Oid table_oid, int id, const char *name, int value);

#ifdef __cplusplus
}
#endif
