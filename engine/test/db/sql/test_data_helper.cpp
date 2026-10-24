/**
 * @file test_data_helper.cpp
 * @brief 数据插入辅助函数实现
 */
#include "test_data_helper.h"

extern "C" {
#include "db/rel.h"
#include "db/heapam.h"
#include "db/catalog.h"
#include <cstring>
}

int insert_test_tuple(Oid table_oid, int id, const char *name, int value) {
    /* 打开表（写模式） */
    Relation rel = relation_open(table_oid, RELMODE_WRITE);
    if (!rel) {
        return -1;
    }

    /* 构造元组数据 */
    struct {
        int32_t id;
        char name[64];
        int32_t value;
    } tuple_data;

    tuple_data.id = id;
    strncpy(tuple_data.name, name, sizeof(tuple_data.name) - 1);
    tuple_data.name[sizeof(tuple_data.name) - 1] = '\0';
    tuple_data.value = value;

    /* 插入元组 */
    int result = heap_insert(rel, &tuple_data, sizeof(tuple_data),
                             0, 0, nullptr);

    /* 关闭表 */
    relation_close(rel, RELMODE_WRITE);
    return result;
}
