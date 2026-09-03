/**
 * @file parse_node.c
 * @brief ParseState 辅助函数实现
 */

#include "db/parser/sql/parse_node.h"
#include "db/parser/sql/makefuncs.h"
#include <stdlib.h>
#include <string.h>

/* NIL 常量（空列表） */
#define NIL ((List *)NULL)

/* ============================================================
 * ParseState 创建/销毁
 * ============================================================ */

/**
 * @brief 创建 ParseState
 */
ParseState *make_parsestate(ParseState *parentParseState) {
    ParseState *pstate = (ParseState *)calloc(1, sizeof(ParseState));

    if (pstate) {
        pstate->parentParseState = parentParseState;
        pstate->p_rtable = NIL;
        pstate->p_joinexprs = NIL;
        pstate->p_namespace = NIL;
        pstate->p_targetlist = NIL;
        pstate->p_paramtypes = NIL;
        pstate->p_numparams = 0;
        pstate->p_ctenamespace = NIL;
        pstate->p_parent_ctenamespace = NIL;
        pstate->p_aggs = NIL;
        pstate->p_groupClause = NIL;
        pstate->p_hasAggs = false;
        pstate->p_hasWindowFuncs = false;
        pstate->p_sublinks = NIL;
        pstate->p_expr_kind = EXPR_KIND_NONE;
        pstate->p_error_msg = NULL;
        pstate->p_error_location = 0;
        pstate->p_sourcetext = NULL;
        pstate->p_relnamespace = NIL;
        pstate->p_lateral_active = NIL;
        pstate->p_hasRowSecurity = false;
        pstate->p_rls_enabled = false;
        pstate->p_rls_support = RLS_NONE;

        /* 从父 ParseState 继承 Catalog */
        if (parentParseState) {
            pstate->p_catalog = parentParseState->p_catalog;
            pstate->p_parent_ctenamespace = parentParseState->p_ctenamespace;
        }
    }

    return pstate;
}

/**
 * @brief 销毁 ParseState
 */
void free_parsestate(ParseState *pstate) {
    if (pstate) {
        if (pstate->p_error_msg) {
            free(pstate->p_error_msg);
        }
        /* 注意：不释放 p_rtable 等列表，它们属于 Query 结构 */
        free(pstate);
    }
}

/* ============================================================
 * RangeTblEntry 创建
 * ============================================================ */

/**
 * @brief 创建表的 RangeTblEntry
 */
RangeTblEntry *addRangeTableEntry(ParseState *pstate, RangeVar *relation,
                                   Alias *alias, bool inh, bool inFromCl) {
    RangeTblEntry *rte = (RangeTblEntry *)calloc(1, sizeof(RangeTblEntry));

    if (rte) {
        rte->type = T_RangeVar;  /* 临时标记，实际应为 T_RangeTblEntry */
        rte->rtekind = RTE_RELATION;

        /* 设置表名和别名 */
        if (relation) {
            rte->relname = relation->relname ? strdup(relation->relname) : NULL;
            rte->relid = InvalidOid;  /* 需要 Catalog 查询 */
        }

        /* 设置别名 */
        if (alias) {
            rte->refname = alias->aliasname ? strdup(alias->aliasname) : NULL;
            rte->eref = alias;
        } else if (relation && relation->alias) {
            rte->refname = strdup(relation->alias);
        } else if (relation && relation->relname) {
            rte->refname = strdup(relation->relname);
        }

        rte->inh = inh;
        rte->lateral = false;
        rte->rellockmode = 0;

        /* 添加到范围表 */
        pstate->p_rtable = lappend(pstate->p_rtable, rte);
    }

    return rte;
}

/**
 * @brief 创建子查询的 RangeTblEntry
 */
RangeTblEntry *addRangeTableEntryForSubquery(ParseState *pstate, Query *subquery,
                                              Alias *alias, bool lateral, bool inFromCl) {
    RangeTblEntry *rte = (RangeTblEntry *)calloc(1, sizeof(RangeTblEntry));

    if (rte) {
        rte->type = T_RangeVar;  /* 临时标记 */
        rte->rtekind = RTE_SUBQUERY;
        rte->subquery = subquery;
        rte->lateral = lateral;

        /* 设置别名 */
        if (alias) {
            rte->refname = alias->aliasname ? strdup(alias->aliasname) : NULL;
            rte->eref = alias;
        }

        /* 添加到范围表 */
        pstate->p_rtable = lappend(pstate->p_rtable, rte);
    }

    return rte;
}

/**
 * @brief 创建函数的 RangeTblEntry
 */
RangeTblEntry *addRangeTableEntryForFunction(ParseState *pstate, List *funcnames,
                                              List *funcexprs, List *coldeflists,
                                              Alias *alias, bool lateral, bool inFromCl) {
    RangeTblEntry *rte = (RangeTblEntry *)calloc(1, sizeof(RangeTblEntry));

    if (rte) {
        rte->type = T_RangeVar;  /* 临时标记 */
        rte->rtekind = RTE_FUNCTION;
        rte->funcexpr = NULL;  /* 需要从 funcexprs 构造 */
        rte->lateral = lateral;

        /* 设置别名 */
        if (alias) {
            rte->refname = alias->aliasname ? strdup(alias->aliasname) : NULL;
            rte->eref = alias;
        }

        /* 添加到范围表 */
        pstate->p_rtable = lappend(pstate->p_rtable, rte);
    }

    return rte;
}

/**
 * @brief 创建 VALUES 的 RangeTblEntry
 */
RangeTblEntry *addRangeTableEntryForValues(ParseState *pstate, List *valueslists,
                                            Alias *alias, bool lateral, bool inFromCl) {
    RangeTblEntry *rte = (RangeTblEntry *)calloc(1, sizeof(RangeTblEntry));

    if (rte) {
        rte->type = T_RangeVar;  /* 临时标记 */
        rte->rtekind = RTE_VALUES;
        rte->values_lists = valueslists;
        rte->lateral = lateral;

        /* 设置别名 */
        if (alias) {
            rte->refname = alias->aliasname ? strdup(alias->aliasname) : NULL;
            rte->eref = alias;
        }

        /* 添加到范围表 */
        pstate->p_rtable = lappend(pstate->p_rtable, rte);
    }

    return rte;
}

/**
 * @brief 创建连接的 RangeTblEntry
 */
RangeTblEntry *addRangeTableEntryForJoin(ParseState *pstate, JoinType jointype,
                                          List *joinusing, Alias *join_using_alias,
                                          List *colnames, List *leftcols, List *rightcols,
                                          Alias *alias, bool inFromCl) {
    RangeTblEntry *rte = (RangeTblEntry *)calloc(1, sizeof(RangeTblEntry));

    if (rte) {
        rte->type = T_RangeVar;  /* 临时标记 */
        rte->rtekind = RTE_JOIN;
        rte->jointype = jointype;
        rte->joinaliasvars = joinusing;
        rte->join_using_alias = (List *)join_using_alias;  /* 类型擦除简化处理 */

        if (leftcols) rte->joinleftcols = list_length(leftcols);
        if (rightcols) rte->joinrightcols = list_length(rightcols);

        /* 设置别名 */
        if (alias) {
            rte->refname = alias->aliasname ? strdup(alias->aliasname) : NULL;
            rte->eref = alias;
        }

        /* 添加到范围表 */
        pstate->p_rtable = lappend(pstate->p_rtable, rte);
    }

    return rte;
}

/**
 * @brief 创建 CTE 的 RangeTblEntry
 */
RangeTblEntry *addRangeTableEntryForCTE(ParseState *pstate, char *ctename,
                                         List *ctecolnames, List *ctecoltypes,
                                         List *ctecoltypmods, List *ctecolcollations,
                                         bool self_reference) {
    RangeTblEntry *rte = (RangeTblEntry *)calloc(1, sizeof(RangeTblEntry));

    if (rte) {
        rte->type = T_RangeVar;  /* 临时标记 */
        rte->rtekind = RTE_CTE;
        rte->ctename = ctename ? strdup(ctename) : NULL;
        rte->ctelevelsup = 0;
        rte->self_reference = self_reference;

        /* 添加到范围表 */
        pstate->p_rtable = lappend(pstate->p_rtable, rte);
    }

    return rte;
}

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * @brief 创建节点（通用分配器）
 * （已在 makefuncs.c 中定义，这里用静态内联避免重复定义）
 */
static inline Node *makeNodeInternal(NodeTag type) {
    Node *n = (Node *)calloc(1, sizeof(Node));
    if (n) {
        n->type = type;
    }
    return n;
}
#define makeNode makeNodeInternal
