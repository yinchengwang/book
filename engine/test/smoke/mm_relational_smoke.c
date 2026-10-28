/**
 * @file mm_relational_smoke.c
 * @brief relational 模态独立冒烟测试
 *
 * relational 不通过 storage_ops_t 注册（MODEL_RELATIONAL 槽位恒空，
 * catalog DDL 无处落地，legacy 亦然），完整执行往返超出本战役范围。
 * 冒烟钉住两层事实（入口为 include/db/sql/sql_driver.h 既有 API）：
 *  1) 解析层活着：合法语句的错误不是"解析失败"（走到了执行阶段）；
 *  2) 非法语句被解析器正确拒绝（grammar 真的在链路上）。
 */
#include "db/mm_storage.h"
#include "db/sql/sql_driver.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_rel") == 0);

    QueryResult *r;

    /* 合法语句：错误不应是解析失败（说明已走过 parser 进入执行阶段） */
    r = execute_sql("SELECT id FROM no_such_table", NULL);
    assert(r != NULL);
    if (r->error_msg) fprintf(stderr, "info: %s\n", r->error_msg);
    assert(r->error_msg != NULL);
    assert(strstr(r->error_msg, "解析失败") == NULL);

    /* 非法语句：必须被解析器拒绝（"SELECT 1" 不符合 grammar 的
     * SELECT ... FROM 形状） */
    r = execute_sql("SELECT 1", NULL);
    assert(r != NULL);
    assert(r->error_msg != NULL);
    assert(strstr(r->error_msg, "解析失败") != NULL);

    assert(mm_storage_shutdown() == 0);
    printf("mm_relational_smoke OK\n");
    return 0;
}
