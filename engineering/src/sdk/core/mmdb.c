/**
 * @file mmdb.c
 * @brief mmdb 生命周期：open / close / last_error / version
 */
#include "sdk/mmdb.h"
#include "sdk/mmdb_error.h"
#include "sdk/mmdb_version.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/sqlite_backend.h"
#include "sdk/impl/collection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 内部辅助                                                            */
/* ------------------------------------------------------------------ */

char* mmdb_strdup_internal(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* p = (char*)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

void mmdb_set_error(mmdb_t* db, int code, const char* msg) {
    if (!db) return;
    db->last_err = code;
    if (db->last_err_msg) {
        free(db->last_err_msg);
        db->last_err_msg = NULL;
    }
    if (msg) {
        size_t n = strlen(msg);
        if (n >= MMDB_ERR_MSG_MAX) n = MMDB_ERR_MSG_MAX - 1;
        db->last_err_msg = (char*)malloc(n + 1);
        if (db->last_err_msg) {
            memcpy(db->last_err_msg, msg, n);
            db->last_err_msg[n] = '\0';
        }
    }
}

/* ------------------------------------------------------------------ */
/* mmdb_open / mmdb_close                                              */
/* ------------------------------------------------------------------ */

mmdb_t* mmdb_open(const char* path, const mmdb_options_t* opts) {
    if (!path) return NULL;

    mmdb_options_t effective = opts ? *opts : (mmdb_options_t)MMDB_OPTIONS_DEFAULT;

    mmdb_t* db = (mmdb_t*)calloc(1, sizeof(mmdb_t));
    if (!db) return NULL;

    db->path = mmdb_strdup_internal(path);
    if (!db->path) {
        free(db);
        return NULL;
    }
    db->options = effective;
    db->last_err = MMDB_OK;

    if (mmdb_rwlock_init(&db->lock) != 0) {
        free(db->path);
        free(db);
        return NULL;
    }

    char err_buf[MMDB_ERR_MSG_MAX] = {0};
    int rc = mmdb_sqlite_open(path, &effective, &db->db, err_buf, sizeof(err_buf));
    if (rc != MMDB_OK) {
        mmdb_set_error(db, rc, err_buf[0] ? err_buf : "open failed");
        mmdb_rwlock_destroy(&db->lock);
        free(db->path);
        free(db);
        return NULL;
    }

    rc = mmdb_sqlite_bootstrap(db->db);
    if (rc != MMDB_OK) {
        mmdb_set_error(db, rc, "bootstrap failed");
        mmdb_sqlite_close(db->db);
        mmdb_rwlock_destroy(&db->lock);
        free(db->path);
        free(db);
        return NULL;
    }

    rc = mmdb_collection_init(db);
    if (rc != MMDB_OK) {
        mmdb_set_error(db, rc, "collection init failed");
        mmdb_sqlite_close(db->db);
        mmdb_rwlock_destroy(&db->lock);
        free(db->path);
        free(db);
        return NULL;
    }

    rc = mmdb_collection_load_all(db);
    if (rc != MMDB_OK) {
        mmdb_set_error(db, rc, "load collections failed");
        mmdb_collection_dispose(db);
        mmdb_sqlite_close(db->db);
        mmdb_rwlock_destroy(&db->lock);
        free(db->path);
        free(db);
        return NULL;
    }

    return db;
}

void mmdb_close(mmdb_t* db) {
    if (!db) return;

    mmdb_collection_dispose(db);
    mmdb_sqlite_close(db->db);
    mmdb_rwlock_destroy(&db->lock);
    free(db->path);
    free(db->last_err_msg);
    free(db);
}

int mmdb_last_error_code(mmdb_t* db) {
    if (!db) return MMDB_ERR_INVALID;
    return db->last_err;
}

const char* mmdb_last_error_message(mmdb_t* db) {
    if (!db) return mmdb_strerror(MMDB_ERR_INVALID);
    if (db->last_err_msg && db->last_err_msg[0]) return db->last_err_msg;
    return mmdb_strerror(db->last_err);
}

/* ------------------------------------------------------------------ */
/* 版本查询                                                            */
/* ------------------------------------------------------------------ */

void mmdb_version(int* major, int* minor, int* patch) {
    if (major) *major = MMDB_VERSION_MAJOR;
    if (minor) *minor = MMDB_VERSION_MINOR;
    if (patch) *patch = MMDB_VERSION_PATCH;
}