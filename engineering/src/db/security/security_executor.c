/**
 * @file src/db/security/security_executor.c
 * @brief 安全执行器实现 - 将安全检查集成到执行节点
 */

#include <db/security/security_executor.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 安全执行器函数实现
 * ============================================================ */

static int security_exec_open(ExecNode *node)
{
    SecurityExecState *state = (SecurityExecState *)node->state;

    if (state == NULL) {
        return -1;
    }

    /* 执行权限检查 */
    if (!security_check_permission(state->security_mgr, state->user_id, state->required_perm)) {
        state->check_passed = false;
        return 0;
    }

    state->check_passed = true;

    /* 调用回调函数（如果存在） */
    if (state->callback != NULL) {
        state->callback(state->user_id, state->required_perm, state->callback_arg);
    }

    /* 打开子节点 */
    if (state->child != NULL && state->child->open != NULL) {
        return state->child->open(state->child);
    }

    return 0;
}

static VectorBlock *security_exec_next(ExecNode *node)
{
    SecurityExecState *state = (SecurityExecState *)node->state;

    if (state == NULL || !state->check_passed) {
        return NULL;
    }

    /* 如果没有子节点，返回 NULL（已结束） */
    if (state->child == NULL) {
        return NULL;
    }

    /* 委托给子节点 */
    if (state->child->next != NULL) {
        return state->child->next(state->child);
    }

    return NULL;
}

static void security_exec_reset(ExecNode *node)
{
    SecurityExecState *state = (SecurityExecState *)node->state;

    if (state == NULL) {
        return;
    }

    /* 重置子节点 */
    if (state->child != NULL && state->child->reset != NULL) {
        state->child->reset(state->child);
    }
}

static void security_exec_close(ExecNode *node)
{
    SecurityExecState *state = (SecurityExecState *)node->state;

    if (state == NULL) {
        return;
    }

    /* 关闭子节点 */
    if (state->child != NULL && state->child->close != NULL) {
        state->child->close(state->child);
    }

    /* 释放状态 */
    free(state);
    node->state = NULL;
}

/* ============================================================
 * 公共函数实现
 * ============================================================ */

ExecNode *exec_create_with_security(security_mgr_t *mgr,
                                    ExecNode *child,
                                    int user_id,
                                    permission_t required_perm)
{
    return exec_create_with_security_callback(mgr, child, user_id, required_perm, NULL, NULL);
}

ExecNode *exec_create_with_security_callback(security_mgr_t *mgr,
                                             ExecNode *child,
                                             int user_id,
                                             permission_t required_perm,
                                             security_check_callback_t callback,
                                             void *callback_arg)
{
    if (mgr == NULL) {
        return NULL;
    }

    if (user_id < 0 || required_perm < 0 || required_perm >= PERM_MAX) {
        return NULL;
    }

    /* 分配执行节点 */
    ExecNode *node = (ExecNode *)malloc(sizeof(ExecNode));
    if (node == NULL) {
        return NULL;
    }

    memset(node, 0, sizeof(ExecNode));

    /* 分配状态结构 */
    SecurityExecState *state = (SecurityExecState *)malloc(sizeof(SecurityExecState));
    if (state == NULL) {
        free(node);
        return NULL;
    }

    memset(state, 0, sizeof(SecurityExecState));
    state->child = child;
    state->security_mgr = mgr;
    state->user_id = user_id;
    state->required_perm = required_perm;
    state->callback = callback;
    state->callback_arg = callback_arg;
    state->check_passed = false;

    /* 设置节点 */
    node->state = state;
    node->node_type = PLAN_SECURITY_CHECK;  /* 使用占位符类型 */
    node->left = child;
    node->right = NULL;
    node->open = security_exec_open;
    node->next = security_exec_next;
    node->reset = security_exec_reset;
    node->close = security_exec_close;

    return node;
}
