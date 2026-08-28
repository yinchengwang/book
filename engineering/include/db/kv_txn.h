#ifndef DB_KV_TXN_H
#define DB_KV_TXN_H

#include "db/kv.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kv_txn_s kv_txn_t;

kv_txn_t *kv_txn_begin(kv_t *db);
int kv_txn_put(kv_txn_t *tx, const void *key, size_t klen,
              const void *value, size_t vlen);
int kv_txn_commit(kv_txn_t *tx);
void kv_txn_rollback(kv_txn_t *tx);
void kv_txn_free(kv_txn_t *tx);

#ifdef __cplusplus
}
#endif

#endif
