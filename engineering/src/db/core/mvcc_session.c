/**
 * @file mvcc_session.c
 * @brief MVCC 线程局部会话状态实现
 */
#include "db/mvcc_session.h"

#ifdef _WIN32
__declspec(thread) static int64_t s_current_xid = 0;
#else
__thread int64_t s_current_xid = 0;
#endif

int64_t mvcc_current_xid(void) {
    return s_current_xid;
}

void mvcc_set_current_xid(int64_t xid) {
    s_current_xid = xid;
}

void mvcc_clear_current_xid(void) {
    s_current_xid = 0;
}