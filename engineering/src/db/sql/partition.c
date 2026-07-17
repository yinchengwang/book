/**
 * @file partition.c
 * @brief 分区表路由机制实现
 *
 * 实现分区表的路由核心逻辑：
 *   - 分区描述符创建与销毁
 *   - 范围分区路由（二分查找）
 *   - 列表分区路由（线性搜索）
 *   - 哈希分区路由（取模计算）
 *
 * 内存管理策略：
 *   - 使用模块级 MemoryContext 管理内存
 *   - 支持 reset/delete 操作清理资源
 */

#include <stdlib.h>
#include <string.h>

#include "db/sql/partition.h"
#include "db/sql/memctx.h"

/* ========================================================================
 * 模块级内存上下文
 * ======================================================================== */

/** 分区模块顶层内存上下文 */
static MemoryContext PartitionContext = NULL;

/**
 * @brief 获取分区模块内存上下文
 *
 * 如果尚未创建，则创建一个顶层内存上下文。
 *
 * @return 分区模块内存上下文
 */
static MemoryContext GetPartitionContext(void) {
    if (PartitionContext == NULL) {
        PartitionContext = AllocSetContextCreate(
            NULL,                        /* 无父上下文 */
            "PartitionContext",         /* 调试用名 */
            0,                           /* minContextSize */
            ALLOCSET_DEFAULT_BLOCK_SIZE, /* initBlockSize */
            ALLOCSET_DEFAULT_BLOCK_SIZE  /* maxBlockSize */
        );
    }
    return PartitionContext;
}

/* ========================================================================
 * 分区边界信息实现
 * ======================================================================== */

/**
 * @brief 创建分区边界信息
 */
PartitionBoundInfo *CreatePartitionBoundInfo(
    PartitionType strategy,
    int ndatums,
    Datum *datums,
    int *indexes) {
    if (ndatums < 0) {
        return NULL;
    }

    MemoryContext ctx = GetPartitionContext();
    if (ctx == NULL) {
        return NULL;
    }

    PartitionBoundInfo *boundinfo =
        (PartitionBoundInfo *)palloc(ctx, sizeof(PartitionBoundInfo));
    if (boundinfo == NULL) {
        return NULL;
    }

    boundinfo->strategy = strategy;
    boundinfo->ndatums = ndatums;

    if (ndatums > 0 && datums != NULL) {
        boundinfo->datums = (Datum *)palloc(ctx, sizeof(Datum) * ndatums);
        if (boundinfo->datums == NULL) {
            pfree(ctx, boundinfo);
            return NULL;
        }
        memcpy(boundinfo->datums, datums, sizeof(Datum) * ndatums);

        /* 分配 is_inclusive 数组并初始化为 true（默认包含边界） */
        boundinfo->is_inclusive = (bool *)palloc(ctx, sizeof(bool) * ndatums);
        if (boundinfo->is_inclusive == NULL) {
            pfree(ctx, boundinfo->datums);
            pfree(ctx, boundinfo);
            return NULL;
        }
        for (int i = 0; i < ndatums; i++) {
            boundinfo->is_inclusive[i] = true;
        }
    } else {
        boundinfo->datums = NULL;
        boundinfo->is_inclusive = NULL;
    }

    if (indexes != NULL && ndatums > 0) {
        boundinfo->indexes = (int *)palloc(ctx, sizeof(int) * ndatums);
        if (boundinfo->indexes == NULL) {
            if (boundinfo->datums != NULL) {
                pfree(ctx, boundinfo->datums);
            }
            if (boundinfo->is_inclusive != NULL) {
                pfree(ctx, boundinfo->is_inclusive);
            }
            pfree(ctx, boundinfo);
            return NULL;
        }
        memcpy(boundinfo->indexes, indexes, sizeof(int) * ndatums);
    } else {
        boundinfo->indexes = NULL;
    }

    return boundinfo;
}

/**
 * @brief 销毁分区边界信息
 *
 * 注意：由于使用模块级内存上下文，这里只做 NULL 检查。
 * 实际内存由 reset_memory(GetPartitionContext()) 统一释放。
 */
void DestroyPartitionBoundInfo(PartitionBoundInfo *boundinfo) {
    if (boundinfo == NULL) {
        return;
    }
    /* 内存由模块级上下文管理，无需逐个释放 */
}

/* ========================================================================
 * 分区描述符实现
 * ======================================================================== */

/**
 * @brief 创建分区描述符
 */
PartitionDesc *CreatePartitionDesc(
    PartitionType parttype,
    Oid relid,
    int nparts) {
    if (nparts < 0) {
        return NULL;
    }

    MemoryContext ctx = GetPartitionContext();
    if (ctx == NULL) {
        return NULL;
    }

    PartitionDesc *partdesc =
        (PartitionDesc *)palloc(ctx, sizeof(PartitionDesc));
    if (partdesc == NULL) {
        return NULL;
    }

    partdesc->type = T_PartitionDesc;
    partdesc->parttype = parttype;
    partdesc->relid = relid;
    partdesc->nparts = nparts;
    partdesc->partnatts = 1;  /* 默认单列分区键 */
    partdesc->boundinfo = NULL;

    if (nparts > 0) {
        partdesc->partattrs = (int16_t *)palloc(ctx, sizeof(int16_t) * nparts);
        if (partdesc->partattrs == NULL) {
            pfree(ctx, partdesc);
            return NULL;
        }
        /* 初始化为默认属性号 */
        for (int i = 0; i < nparts; i++) {
            partdesc->partattrs[i] = 1;
        }

        partdesc->partopfamily = (Oid *)palloc(ctx, sizeof(Oid) * nparts);
        if (partdesc->partopfamily == NULL) {
            pfree(ctx, partdesc->partattrs);
            pfree(ctx, partdesc);
            return NULL;
        }

        partdesc->partopcintype = (Oid *)palloc(ctx, sizeof(Oid) * nparts);
        if (partdesc->partopcintype == NULL) {
            pfree(ctx, partdesc->partopfamily);
            pfree(ctx, partdesc->partattrs);
            pfree(ctx, partdesc);
            return NULL;
        }
    } else {
        partdesc->partattrs = NULL;
        partdesc->partopfamily = NULL;
        partdesc->partopcintype = NULL;
    }

    return partdesc;
}

/**
 * @brief 销毁分区描述符
 */
void DestroyPartitionDesc(PartitionDesc *partdesc) {
    if (partdesc == NULL) {
        return;
    }

    if (partdesc->boundinfo != NULL) {
        DestroyPartitionBoundInfo(partdesc->boundinfo);
        partdesc->boundinfo = NULL;
    }
    /* partattrs、partopfamily、partopcintype 由模块级上下文管理 */
}

/**
 * @brief 获取分区描述符的分类型
 */
PartitionType GetPartitionType(const PartitionDesc *partdesc) {
    if (partdesc == NULL) {
        return PARTITION_TYPE_RANGE;  /* 默认类型 */
    }
    return partdesc->parttype;
}

/**
 * @brief 获取分区数量
 */
int GetPartitionCount(const PartitionDesc *partdesc) {
    if (partdesc == NULL) {
        return 0;
    }
    return partdesc->nparts;
}

/* ========================================================================
 * 分区路由上下文实现
 * ======================================================================== */

/**
 * @brief 创建分区路由上下文
 */
PartitionRoutingCtx *CreatePartitionRoutingCtx(
    PartitionDesc *partdesc,
    EState *estate) {
    if (partdesc == NULL) {
        return NULL;
    }

    MemoryContext ctx = GetPartitionContext();
    if (ctx == NULL) {
        return NULL;
    }

    PartitionRoutingCtx *prctx =
        (PartitionRoutingCtx *)palloc(ctx, sizeof(PartitionRoutingCtx));
    if (prctx == NULL) {
        return NULL;
    }

    prctx->type = T_PartitionRoutingCtx;
    prctx->partdesc = partdesc;
    prctx->estate = estate;
    prctx->root_rrinfo = NULL;
    prctx->nparts = partdesc->nparts;

    if (partdesc->nparts > 0) {
        prctx->part_rrinfos = (ResultRelInfo **)palloc(
            ctx, sizeof(ResultRelInfo *) * partdesc->nparts);
        if (prctx->part_rrinfos == NULL) {
            pfree(ctx, prctx);
            return NULL;
        }
        memset(prctx->part_rrinfos, 0,
               sizeof(ResultRelInfo *) * partdesc->nparts);
    } else {
        prctx->part_rrinfos = NULL;
    }

    return prctx;
}

/**
 * @brief 销毁分区路由上下文
 */
void DestroyPartitionRoutingCtx(PartitionRoutingCtx *prctx) {
    if (prctx == NULL) {
        return;
    }
    /* 内存由模块级上下文管理 */
}

/* ========================================================================
 * 分区边界比较实现
 * ======================================================================== */

/**
 * @brief 比较分区边界值（整数版本）
 */
static int PartitionBoundCmpInt(Datum key, Datum bound) {
    int64_t key_val = (int64_t)key;
    int64_t bound_val = (int64_t)bound;

    if (key_val < bound_val) {
        return -1;
    } else if (key_val > bound_val) {
        return 1;
    }
    return 0;
}

/**
 * @brief 比较分区边界值（浮点版本）
 */
static int PartitionBoundCmpFloat(Datum key, Datum bound) {
    double key_val;
    double bound_val;

    /* 将 Datum 转换为 double */
    memcpy(&key_val, &key, sizeof(double));
    memcpy(&bound_val, &bound, sizeof(double));

    if (key_val < bound_val) {
        return -1;
    } else if (key_val > bound_val) {
        return 1;
    }
    return 0;
}

/**
 * @brief 比较分区边界值
 */
int PartitionBoundCmp(Datum key, Datum bound, bool is_int) {
    if (is_int) {
        return PartitionBoundCmpInt(key, bound);
    }
    return PartitionBoundCmpFloat(key, bound);
}

/* ========================================================================
 * 范围分区路由实现
 * ======================================================================== */

/**
 * @brief 范围分区路由
 *
 * 使用二分查找确定分区键值所属的分区。
 * 假设边界值已排序，且每个分区的上界为 datums[i]。
 * 边界比较规则：
 *   - 值 < datums[0]: 属于分区 0
 *   - datums[i-1] <= 值 < datums[i]: 属于分区 i
 *   - 值 >= datums[n-1]: 属于分区 n
 */
int PartitionRangeRouting(
    const PartitionBoundInfo *boundinfo,
    Datum partkey) {
    if (boundinfo == NULL || boundinfo->ndatums <= 0) {
        return -1;  /* 无分区或无效边界 */
    }

    int ndatums = boundinfo->ndatums;
    Datum *datums = boundinfo->datums;
    bool *is_inclusive = boundinfo->is_inclusive;

    /* 二分查找边界 */
    int low = 0;
    int high = ndatums - 1;

    /* 特殊情况：第一个边界 */
    int cmp_first = PartitionBoundCmp(partkey, datums[0], true);
    if (cmp_first < 0) {
        return 0;  /* 小于第一个边界 */
    }
    if (cmp_first == 0 && is_inclusive != NULL && is_inclusive[0]) {
        return 0;  /* 等于第一个边界且包含 */
    }

    /* 特殊情况：最后一个边界 */
    int cmp_last = PartitionBoundCmp(partkey, datums[high], true);
    if (cmp_last >= 0) {
        /* 大于等于最后边界，检查是否等于且包含 */
        if (cmp_last == 0 && is_inclusive != NULL && is_inclusive[high]) {
            return high;
        }
        return high + 1;  /* 大于最后边界，落在最后一个分区之后 */
    }

    /* 二分查找中间位置 */
    while (low < high - 1) {
        int mid = (low + high) / 2;
        int cmp = PartitionBoundCmp(partkey, datums[mid], true);

        if (cmp < 0) {
            high = mid;
        } else if (cmp > 0) {
            low = mid;
        } else {
            /* 正好等于边界值 */
            if (is_inclusive != NULL && is_inclusive[mid]) {
                return mid;  /* 包含此边界 */
            }
            return mid + 1;  /* 不包含则落入下一分区 */
        }
    }

    /* low < high - 1 循环结束时 low 指向目标分区 */
    return low + 1;
}

/* ========================================================================
 * 列表分区路由实现
 * ======================================================================== */

/**
 * @brief 列表分区路由
 *
 * 线性搜索匹配分区键值所在的枚举分区。
 * 每个 datums[i] 对应一个枚举值，匹配到则返回对应分区。
 */
int PartitionListRouting(
    const PartitionBoundInfo *boundinfo,
    Datum partkey) {
    if (boundinfo == NULL || boundinfo->ndatums <= 0) {
        return -1;  /* 无分区或无效边界 */
    }

    int ndatums = boundinfo->ndatums;
    Datum *datums = boundinfo->datums;
    int *indexes = boundinfo->indexes;

    /* 线性搜索查找匹配值 */
    for (int i = 0; i < ndatums; i++) {
        int cmp = PartitionBoundCmp(partkey, datums[i], true);
        if (cmp == 0) {
            /* 找到匹配值 */
            if (indexes != NULL) {
                return indexes[i];
            }
            return i;
        }
    }

    return -1;  /* 未找到匹配值 */
}

/* ========================================================================
 * 哈希分区路由实现
 * ======================================================================== */

/**
 * @brief 哈希分区路由
 *
 * 通过取模运算确定目标分区。
 * 使用简单的模运算：partition = hash(key) % nparts
 * 为简化实现，直接使用 key 值本身进行取模。
 */
int PartitionHashRouting(
    int nparts,
    Datum partkey) {
    if (nparts <= 0) {
        return -1;  /* 无效分区数 */
    }

    int64_t key_val = (int64_t)partkey;
    int partition = (int)(key_val % nparts);

    /* 确保结果为非负数 */
    if (partition < 0) {
        partition += nparts;
    }

    return partition;
}

/* ========================================================================
 * 分区路由主函数
 * ======================================================================== */

/**
 * @brief 路由分区键值到目标分区
 *
 * 根据分区类型调用对应的路由函数。
 */
int PartitionRouting(
    const PartitionDesc *partdesc,
    Datum *partkey,
    int nkeys) {
    if (partdesc == NULL || partkey == NULL || nkeys <= 0) {
        return -1;  /* 无效参数 */
    }

    /* 使用第一个分区键值进行路由（简化处理多列分区键） */
    Datum key = partkey[0];

    switch (partdesc->parttype) {
        case PARTITION_TYPE_RANGE:
            return PartitionRangeRouting(partdesc->boundinfo, key);

        case PARTITION_TYPE_LIST:
            return PartitionListRouting(partdesc->boundinfo, key);

        case PARTITION_TYPE_HASH:
            return PartitionHashRouting(partdesc->nparts, key);

        default:
            return -1;  /* 未知分区类型 */
    }
}

/**
 * @brief 检查值是否为 NULL 分区的候选
 *
 * NULL 值在某些分区策略中有特殊处理。
 * 当前实现：如果分区键值为 0（常见 NULL 表示），返回 true。
 */
bool IsNullPartition(const PartitionDesc *partdesc, Datum *partkey) {
    if (partdesc == NULL || partkey == NULL) {
        return false;
    }

    /* 检查第一个分区键值是否为 0（NULL 的简化表示） */
    if (partkey[0] == 0) {
        return true;
    }

    return false;
}
