// faiss_hnsw_visited_table.c
// VisitedTable 实现：参考 FAISS HNSW::VisitedTable 重写为纯 C 版本
//
// 关键设计：
//   - 使用 visid 数组（大小 = num）记录每个节点最近被访问的 visno
//   - 通过单调递增 curvisid 实现"批量重置"：访问检查只比较 visid[id] == curvisid + 1
//   - get/set 操作 O(1)，不需要遍历清空数组

#include "db/index/vector_index/faiss_hnsw/faiss_hnsw_visited_table.h"

#include <stdlib.h>
#include <string.h>

// 创建访问表
int32_t faiss_hnsw_visited_table_create(faiss_hnsw_visited_table_t **vt, int32_t n) {
    if (!vt || n <= 0) {
        return -1;
    }

    faiss_hnsw_visited_table_t *t = (faiss_hnsw_visited_table_t *)calloc(1, sizeof(faiss_hnsw_visited_table_t));
    if (!t) {
        return -1;
    }

    t->curvisid = 0;
    t->num = n;
    t->visid = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    if (!t->visid) {
        free(t);
        return -1;
    }

    // 初始化为 0，所有节点均未访问
    memset(t->visid, 0, (size_t)n * sizeof(int32_t));

    *vt = t;
    return 0;
}

// 销毁访问表
void faiss_hnsw_visited_table_drop(faiss_hnsw_visited_table_t *vt) {
    if (!vt) {
        return;
    }
    free(vt->visid);
    free(vt);
}

// 查询节点是否已被访问（本次查询中）
bool faiss_hnsw_visited_table_get(faiss_hnsw_visited_table_t *vt, int32_t id) {
    if (!vt || id < 0 || id >= vt->num) {
        return false;
    }
    // 已访问条件：visid[id] == curvisid + 1
    return vt->visid[id] == vt->curvisid + 1;
}

// 标记节点为已访问
void faiss_hnsw_visited_table_set(faiss_hnsw_visited_table_t *vt, int32_t id) {
    if (!vt || id < 0 || id >= vt->num) {
        return;
    }
    vt->visid[id] = vt->curvisid + 1;
}

// 重置访问表（递增 curvisid，使所有旧标记失效）
void faiss_hnsw_visited_table_reset(faiss_hnsw_visited_table_t *vt) {
    if (!vt) {
        return;
    }
    // 注意：curvisid 是单调递增的，永不归零（除非溢出）
    // 当 curvisid 接近 INT32_MAX 时需要全部清零（暂不实现，正常使用不会达到）
    vt->curvisid += 2;
    if (vt->curvisid < 0) {
        // 溢出回卷，全部清零并归零
        memset(vt->visid, 0, (size_t)vt->num * sizeof(int32_t));
        vt->curvisid = 0;
    }
}
