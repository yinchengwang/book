#ifndef DB_FSM_H
#define DB_FSM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* C7.3: Free Space Map — 每页 1 bit 记录是否有空闲 */
typedef struct fsm_s fsm_t;

fsm_t *fsm_create(uint32_t n_pages);
void fsm_destroy(fsm_t *fsm);

/* 标记页面空闲 */
int fsm_mark_free(fsm_t *fsm, uint32_t page_id, bool is_free);

/* 找一个有空闲的页面（best-fit：先找 is_free=true 的） */
int32_t fsm_find_free(fsm_t *fsm);

/* FSM 与 page_id 范围查询 */
uint32_t fsm_n_pages(const fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif
