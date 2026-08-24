/**
 * @file collection.c
 * @brief Collection CRUD 实现（注册表与缓存）
 *
 * Collection 元数据存储在 mmdb_collections 系统表中。
 * 每个 Collection 按模型在首次创建时建立对应的数据表（DDL 由各模型模块负责）。
 */
#include "sdk/mmdb.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/collection.h"
#include "sdk/impl/schema.h"
#include "sdk/impl/sqlite_backend.h"
#include "sdk/impl/vectors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* 内部辅助                                                            */
/* ------------------------------------------------------------------ */

/* 拷贝 schema 内容（含 fields 数组） */
static int schema_deep_copy(mmdb_schema_t* dst, const mmdb_schema_t* src) {
    dst->model = src->model;
    dst->vector_dim = src->vector_dim;
    dst->field_count = 0;
    dst->fields = NULL;
    if (src->field_count == 0) return MMDB_OK;
    dst->fields = (mmdb_field_def_t*)calloc(src->field_count,
                                            sizeof(mmdb_field_def_t));
    if (!dst->fields) return MMDB_ERR_NOMEM;
    dst->field_count = src->field_count;
    for (size_t i = 0; i < src->field_count; i++) {
        if (src->fields[i].name) {
            dst->fields[i].name = mmdb_strdup_internal(src->fields[i].name);
            if (!dst->fields[i].name) goto fail;
        }
        if (src->fields[i].default_value_json) {
            dst->fields[i].default_value_json =
                mmdb_strdup_internal(src->fields[i].default_value_json);
            if (!dst->fields[i].default_value_json) goto fail;
        }
        dst->fields[i].type = src->fields[i].type;
        dst->fields[i].nullable = src->fields[i].nullable;
    }
    return MMDB_OK;
fail:
    for (size_t i = 0; i < dst->field_count; i++) {
        free((void*)dst->fields[i].name);
        free((void*)dst->fields[i].default_value_json);
    }
    free(dst->fields);
    dst->fields = NULL;
    dst->field_count = 0;
    return MMDB_ERR_NOMEM;
}

static void schema_free(mmdb_schema_t* s) {
    if (!s) return;
    if (s->fields) {
        for (size_t i = 0; i < s->field_count; i++) {
            free((void*)s->fields[i].name);
            free((void*)s->fields[i].default_value_json);
        }
        free(s->fields);
        s->fields = NULL;
    }
    s->field_count = 0;
}

static int collections_push(mmdb_t* db, mmdb_collection_t* c) {
    size_t new_cap = db->collection_count + 1;
    mmdb_collection_t** narr = (mmdb_collection_t**)realloc(
        db->collections, new_cap * sizeof(mmdb_collection_t*));
    if (!narr) return MMDB_ERR_NOMEM;
    db->collections = narr;
    db->collections[db->collection_count++] = c;
    return MMDB_OK;
}

/* ------------------------------------------------------------------ */
/* 注册表操作                                                          */
/* ------------------------------------------------------------------ */

int mmdb_collection_init(mmdb_t* db) {
    if (!db) return MMDB_ERR_INVALID;
    db->collections = NULL;
    db->collection_count = 0;
    return MMDB_OK;
}

int mmdb_collection_insert_meta(mmdb_t* db, const char* name,
                                mmdb_model_t model, const char* schema_json,
                                size_t vector_dim) {
    const char* sql =
        "INSERT INTO mmdb_collections(name, model, schema_json, vector_dim, "
        "created_at) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt =
        mmdb_sqlite_prepare(db->db, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;
    int rc = MMDB_OK;
    do {
        if (mmdb_sqlite_bind_text(stmt, 1, name) != MMDB_OK) { rc = MMDB_ERR_INVALID; break; }
        if (mmdb_sqlite_bind_int(stmt, 2, (int64_t)model) != MMDB_OK) { rc = MMDB_ERR_INVALID; break; }
        if (mmdb_sqlite_bind_text(stmt, 3, schema_json ? schema_json : "") != MMDB_OK) { rc = MMDB_ERR_INVALID; break; }
        if (mmdb_sqlite_bind_int(stmt, 4, (int64_t)vector_dim) != MMDB_OK) { rc = MMDB_ERR_INVALID; break; }
        if (mmdb_sqlite_bind_int(stmt, 5, (int64_t)time(NULL)) != MMDB_OK) { rc = MMDB_ERR_INVALID; break; }
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            rc = (sqlite3_errcode(db->db) == SQLITE_CONSTRAINT)
                     ? MMDB_ERR_ALREADY
                     : MMDB_ERR_IO;
            break;
        }
        rc = MMDB_OK;
    } while (0);
    sqlite3_finalize(stmt);
    return rc;
}

int mmdb_collection_delete_meta(mmdb_t* db, const char* name) {
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(
        db->db, "DELETE FROM mmdb_collections WHERE name = ?;", NULL, 0);
    if (!stmt) return MMDB_ERR_IO;
    int rc = MMDB_OK;
    mmdb_sqlite_bind_text(stmt, 1, name);
    if (sqlite3_step(stmt) != SQLITE_DONE) rc = MMDB_ERR_IO;
    sqlite3_finalize(stmt);
    return rc;
}

mmdb_collection_t* mmdb_collection_find(mmdb_t* db, const char* name) {
    if (!db || !name) return NULL;
    for (size_t i = 0; i < db->collection_count; i++) {
        if (strcmp(db->collections[i]->name, name) == 0) {
            return db->collections[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* 启动时加载所有 collection                                            */
/* ------------------------------------------------------------------ */

int mmdb_collection_load_all(mmdb_t* db) {
    if (!db) return MMDB_ERR_INVALID;
    sqlite3_stmt* stmt = mmdb_sqlite_prepare(
        db->db, "SELECT name, model, schema_json, vector_dim FROM "
                "mmdb_collections;",
        NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    int rc = MMDB_OK;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        int model_i = sqlite3_column_int(stmt, 1);
        const char* schema_json =
            (const char*)sqlite3_column_text(stmt, 2);
        int vdim = sqlite3_column_int(stmt, 3);
        if (!name) continue;

        mmdb_collection_t* c =
            (mmdb_collection_t*)calloc(1, sizeof(mmdb_collection_t));
        if (!c) { rc = MMDB_ERR_NOMEM; break; }

        c->db = db;
        c->sdb = db->db;
        c->coll_lock = &db->lock;
        c->name = mmdb_strdup_internal(name);
        c->model = (mmdb_model_t)model_i;
        c->schema.vector_dim = (size_t)vdim;
        if (!c->name) {
            free(c);
            rc = MMDB_ERR_NOMEM;
            break;
        }
        if (schema_json && mmdb_schema_from_json(schema_json, &c->schema) != MMDB_OK) {
            free(c->name);
            free(c);
            rc = MMDB_ERR_CORRUPT;
            break;
        }
        if (collections_push(db, c) != MMDB_OK) {
            schema_free(&c->schema);
            free(c->name);
            free(c);
            rc = MMDB_ERR_NOMEM;
            break;
        }
    }
    sqlite3_finalize(stmt);

    /* Phase 2: 启动时为向量 collection 预构建 HNSW 索引（N >= 阈值） */
    for (size_t i = 0; i < db->collection_count; i++) {
        mmdb_collection_t* c = db->collections[i];
        if (c->model == MMDB_MODEL_VECTOR) {
            mmdb_vectors_hnsw_ensure(c);
        }
    }

    return rc;
}

/* ------------------------------------------------------------------ */
/* 释放                                                                */
/* ------------------------------------------------------------------ */

void mmdb_collection_dispose(mmdb_t* db) {
    if (!db || !db->collections) return;
    for (size_t i = 0; i < db->collection_count; i++) {
        mmdb_collection_t* c = db->collections[i];
        /* Phase 2: 释放 HNSW 索引内存 */
        mmdb_vectors_hnsw_free(c);
        schema_free(&c->schema);
        free(c->name);
        free(c);
    }
    free(db->collections);
    db->collections = NULL;
    db->collection_count = 0;
}

/* ------------------------------------------------------------------ */
/* 公开 API                                                            */
/* ------------------------------------------------------------------ */

mmdb_collection_t* mmdb_collection_get(mmdb_t* db, const char* name) {
    if (!db || !name) return NULL;
    return mmdb_collection_find(db, name);
}

mmdb_collection_t* mmdb_collection_create(mmdb_t* db, const char* name,
                                          const mmdb_schema_t* schema) {
    if (!db || !name || !schema) return NULL;

    int vrc = mmdb_schema_validate(schema);
    if (vrc != MMDB_OK) {
        mmdb_set_error(db, vrc, "schema validation failed");
        return NULL;
    }

    /* 已存在则返回错误 */
    if (mmdb_collection_find(db, name)) {
        mmdb_set_error(db, MMDB_ERR_ALREADY, "collection already exists");
        return NULL;
    }

    char* schema_json = mmdb_schema_to_json(schema);
    if (!schema_json) {
        mmdb_set_error(db, MMDB_ERR_NOMEM, "schema to json OOM");
        return NULL;
    }

    mmdb_rwlock_wrlock(&db->lock);
    int rc = mmdb_collection_insert_meta(db, name, schema->model, schema_json,
                                         schema->vector_dim);
    if (rc != MMDB_OK) {
        mmdb_rwlock_unlock(&db->lock, 1);
        free(schema_json);
        mmdb_set_error(db, rc, "insert collection meta failed");
        return NULL;
    }
    free(schema_json);

    mmdb_collection_t* c =
        (mmdb_collection_t*)calloc(1, sizeof(mmdb_collection_t));
    if (!c) {
        mmdb_rwlock_unlock(&db->lock, 1);
        mmdb_collection_delete_meta(db, name); /* 回滚 */
        mmdb_set_error(db, MMDB_ERR_NOMEM, "alloc coll");
        return NULL;
    }
    c->db = db;
    c->sdb = db->db;
    c->coll_lock = &db->lock;
    c->name = mmdb_strdup_internal(name);
    if (!c->name) {
        mmdb_rwlock_unlock(&db->lock, 1);
        mmdb_collection_delete_meta(db, name);
        free(c);
        mmdb_set_error(db, MMDB_ERR_NOMEM, "dup name");
        return NULL;
    }
    c->model = schema->model;
    if (schema_deep_copy(&c->schema, schema) != MMDB_OK) {
        mmdb_rwlock_unlock(&db->lock, 1);
        mmdb_collection_delete_meta(db, name);
        free(c->name);
        free(c);
        mmdb_set_error(db, MMDB_ERR_NOMEM, "copy schema");
        return NULL;
    }
    if (collections_push(db, c) != MMDB_OK) {
        mmdb_rwlock_unlock(&db->lock, 1);
        schema_free(&c->schema);
        free(c->name);
        free(c);
        mmdb_collection_delete_meta(db, name);
        mmdb_set_error(db, MMDB_ERR_NOMEM, "push collection");
        return NULL;
    }
    mmdb_rwlock_unlock(&db->lock, 1);
    return c;
}

void mmdb_collection_drop(mmdb_collection_t* coll) {
    if (!coll || !coll->db) return;
    mmdb_t* db = coll->db;
    mmdb_rwlock_wrlock(&db->lock);
    /* 从缓存中移除 */
    for (size_t i = 0; i < db->collection_count; i++) {
        if (db->collections[i] == coll) {
            /* 后项前移 */
            for (size_t j = i; j + 1 < db->collection_count; j++) {
                db->collections[j] = db->collections[j + 1];
            }
            db->collection_count--;
            break;
        }
    }
    mmdb_collection_delete_meta(db, coll->name);
    schema_free(&coll->schema);
    free(coll->name);
    free(coll);
    mmdb_rwlock_unlock(&db->lock, 1);
}

const char* mmdb_collection_name(mmdb_collection_t* coll) {
    return coll ? coll->name : NULL;
}

mmdb_t* mmdb_collection_db(mmdb_collection_t* coll) {
    return coll ? coll->db : NULL;
}