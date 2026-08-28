#include "db/kv_txn.h"
#include "db/core/log.h"

#include <stdlib.h>

#define KV_TXN_MAX_OPS 256

typedef struct kv_txn_op_s {
    char *key;
    size_t klen;
    char *value;
    size_t vlen;
} kv_txn_op_t;

struct kv_txn_s {
    kv_t *db;
    kv_txn_op_t ops[KV_TXN_MAX_OPS];
    int n_ops;
    bool active;
};

kv_txn_t *kv_txn_begin(kv_t *db) {
    if (!db) return NULL;
    kv_txn_t *tx = calloc(1, sizeof(*tx));
    if (!tx) return NULL;
    tx->db = db;
    tx->active = true;
    LOG_INFO("kv_txn_begin");
    return tx;
}

int kv_txn_put(kv_txn_t *tx, const void *key, size_t klen,
              const void *value, size_t vlen) {
    if (!tx || !tx->active || tx->n_ops >= KV_TXN_MAX_OPS) return -1;
    kv_txn_op_t *op = &tx->ops[tx->n_ops++];
    op->key = malloc(klen); op->klen = klen;
    if (klen > 0) memcpy(op->key, key, klen);
    op->value = malloc(vlen); op->vlen = vlen;
    if (vlen > 0) memcpy(op->value, value, vlen);
    return 0;
}

int kv_txn_commit(kv_txn_t *tx) {
    if (!tx || !tx->active) return -1;
    for (int i = 0; i < tx->n_ops; ++i) {
        kv_result_t rc = kv_put(tx->db, tx->ops[i].key, tx->ops[i].klen,
                                tx->ops[i].value, tx->ops[i].vlen);
        if (rc != KV_OK) {
            LOG_WARN("kv_txn_commit: op %d 失败 rc=%d", i, rc);
            tx->active = false;
            return -1;
        }
    }
    tx->active = false;
    LOG_INFO("kv_txn_commit: %d op", tx->n_ops);
    return 0;
}

void kv_txn_rollback(kv_txn_t *tx) {
    if (!tx) return;
    tx->active = false;
}

void kv_txn_free(kv_txn_t *tx) {
    if (!tx) return;
    for (int i = 0; i < tx->n_ops; ++i) {
        free(tx->ops[i].key);
        free(tx->ops[i].value);
    }
    free(tx);
}
