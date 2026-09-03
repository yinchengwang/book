#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/rel.h"

int main() {
    printf("=== 开始调试 ===\n");
    
    // 初始化
    if (catalog_init() != 0) {
        printf("Catalog 初始化失败\n");
        return -1;
    }
    printf("Catalog 初始化成功\n");
    
    if (buf_init(1024) != 0) {
        printf("Buffer Pool 初始化失败\n");
        return -1;
    }
    printf("Buffer Pool 初始化成功\n");
    
    if (heapam_init() != 0) {
        printf("Heap AM 初始化失败\n");
        return -1;
    }
    printf("Heap AM 初始化成功\n");
    
    if (rel_init() != 0) {
        printf("Relation 管理器初始化失败\n");
        return -1;
    }
    printf("Relation 管理器初始化成功\n");
    
    // 创建表
    column_def_t columns[2];
    memset(columns, 0, sizeof(columns));
    strncpy(columns[0].name, "id", NAMEDATALEN);
    columns[0].type_oid = 23;
    strncpy(columns[1].name, "value", NAMEDATALEN);
    columns[1].type_oid = 23;
    
    Oid table_oid = catalog_create_table("debug_test", columns, 2);
    printf("表 OID: %u\n", table_oid);
    
    if (table_oid == InvalidOid) {
        printf("表创建失败\n");
        return -1;
    }
    
    // 打开 Relation
    Relation rel = relation_open(table_oid, RELMODE_WRITE);
    if (!rel) {
        printf("Relation 打开失败\n");
        return -1;
    }
    printf("Relation 打开成功, rd_nblocks=%u, rd_relfilenode=%u\n", 
           rel->rd_nblocks, rel->rd_relfilenode);
    
    // 插入数据
    int data[2] = {1, 100};
    int result = heap_insert(rel, data, sizeof(data), 0, 0, NULL);
    printf("插入结果: %d, rd_nblocks=%u\n", result, rel->rd_nblocks);
    
    // 检查 Buffer Pool 统计
    buf_stats_t stats;
    buf_get_stats(&stats);
    printf("Buffer stats: hits=%llu, misses=%llu, num_buf_used=%d\n",
           (unsigned long long)stats.hits, 
           (unsigned long long)stats.misses,
           stats.num_buf_used);
    
    relation_close(rel, RELMODE_WRITE);
    
    // 重新打开并扫描
    rel = relation_open(table_oid, RELMODE_READ);
    printf("重新打开 Relation, rd_nblocks=%u\n", rel->rd_nblocks);
    
    TableScanDesc scan = table_beginscan(rel, 0, NULL);
    if (!scan) {
        printf("扫描初始化失败\n");
        return -1;
    }
    printf("扫描初始化成功\n");
    
    int count = 0;
    void *tuple;
    while ((tuple = table_getnext(scan)) != NULL) {
        printf("  扫描到元组 %d: %p\n", count, tuple);
        count++;
    }
    printf("扫描完成, 共 %d 个元组\n", count);
    
    table_endscan(scan);
    relation_close(rel, RELMODE_READ);
    
    // 清理
    rel_shutdown();
    heapam_shutdown();
    buf_shutdown();
    catalog_shutdown();
    
    printf("=== 调试完成 ===\n");
    return 0;
}
