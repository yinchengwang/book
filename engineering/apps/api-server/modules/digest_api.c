 /**
  * digest_api.c
  * 
  * 每日速览只读 API
  * 
  * 策略：daily-digest Python 后端已成熟，C 端作为只读网关，
  * 将 Python 后端的 digest_items 表通过 SQLite 共享。
  * 
  * 实际运行时，daily-digest Python 进程和 C api-server 共享同一个
  * SQLite 数据库文件（WAL 模式支持并发读）。
  * 
  * 路由：
  *   GET /api/digest/today
  *   GET /api/digest/items
  *   GET /api/digest/collections
  *   POST /api/digest/action
  */
 
#include "digest_api.h"
#include "../../common/http_server.h"
#include "../../common/db_pool.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int ensure_digest_tables(void) {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS digest_items ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  source          TEXT NOT NULL,"
        "  source_id       TEXT NOT NULL,"
        "  title           TEXT NOT NULL,"
        "  url             TEXT,"
        "  summary         TEXT,"
        "  raw_content     TEXT,"
        "  category        TEXT,"
        "  tags            TEXT DEFAULT '[]',"
        "  score           REAL DEFAULT 0.0,"
        "  published       TEXT,"
        "  source_weight   REAL DEFAULT 1.0,"
        "  created_at      TEXT NOT NULL,"
        "  UNIQUE(source, source_id)"
        ");"
        "CREATE TABLE IF NOT EXISTS digest_subscriptions ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name            TEXT NOT NULL,"
        "  url             TEXT NOT NULL,"
        "  type            TEXT NOT NULL DEFAULT 'rss',"
        "  category        TEXT,"
        "  enabled         INTEGER NOT NULL DEFAULT 1,"
        "  fetch_interval  INTEGER DEFAULT 3600,"
        "  created_at      TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS digest_collections ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name            TEXT NOT NULL,"
        "  description     TEXT,"
        "  item_ids        TEXT NOT NULL DEFAULT '[]',"
        "  created_at      TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS digest_user_actions ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  item_id         INTEGER NOT NULL,"
        "  action          TEXT NOT NULL,"
        "  created_at      TEXT NOT NULL"
        ");";
    if (db_exec(sql) != SQLITE_OK) {
        fprintf(stderr, "[digest_api] failed to create tables\n");
        return -1;
    }
    return 0;
}

static void now_str(char *buf, size_t len) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%S", tm);
}

/* handler: GET /api/digest/today */
static void handle_digest_today(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    ensure_digest_tables();
    
    /* 获取今天创建的 items */
    char today[16];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(today, sizeof(today), "%Y-%m-%d", tm);
    
    char sql[4096];
    snprintf(sql, sizeof(sql),
        "SELECT id, source, source_id, title, url, summary, category, tags, "
        "score, published, created_at FROM digest_items "
        "WHERE date(created_at) = '%s' ORDER BY score DESC, created_at DESC LIMIT 50",
        today);
    
    sqlite3_stmt *stmt = db_prepare(sql);
    if (!stmt) { http_respond_error(resp, 500, "query failed"); return; }
    
    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *j = cJSON_CreateObject();
        cJSON_AddNumberToObject(j, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(j, "source", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(j, "source_id", (const char *)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(j, "title", (const char *)sqlite3_column_text(stmt, 3));
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL)
            cJSON_AddStringToObject(j, "url", (const char *)sqlite3_column_text(stmt, 4));
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL)
            cJSON_AddStringToObject(j, "summary", (const char *)sqlite3_column_text(stmt, 5));
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL)
            cJSON_AddStringToObject(j, "category", (const char *)sqlite3_column_text(stmt, 6));
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL)
            cJSON_AddRawToObject(j, "tags", (const char *)sqlite3_column_text(stmt, 7));
        cJSON_AddNumberToObject(j, "score", sqlite3_column_double(stmt, 8));
        if (sqlite3_column_type(stmt, 9) != SQLITE_NULL)
            cJSON_AddStringToObject(j, "published", (const char *)sqlite3_column_text(stmt, 9));
        cJSON_AddStringToObject(j, "created_at", (const char *)sqlite3_column_text(stmt, 10));
        cJSON_AddItemToArray(arr, j);
    }
    sqlite3_finalize(stmt);
    
    char *json = cJSON_PrintUnformatted(arr);
    http_respond_json(resp, 200, json);
    cJSON_Delete(arr);
    free(json);
}

/* handler: GET /api/digest/items */
static void handle_digest_items(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    ensure_digest_tables();
    
    sqlite3_stmt *stmt = db_prepare(
        "SELECT id, source, source_id, title, url, summary, category, tags, "
        "score, published, created_at FROM digest_items "
        "ORDER BY created_at DESC LIMIT 200");
    if (!stmt) { http_respond_error(resp, 500, "query failed"); return; }
    
    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *j = cJSON_CreateObject();
        cJSON_AddNumberToObject(j, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(j, "source", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(j, "source_id", (const char *)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(j, "title", (const char *)sqlite3_column_text(stmt, 3));
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL)
            cJSON_AddStringToObject(j, "url", (const char *)sqlite3_column_text(stmt, 4));
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL)
            cJSON_AddStringToObject(j, "summary", (const char *)sqlite3_column_text(stmt, 5));
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL)
            cJSON_AddStringToObject(j, "category", (const char *)sqlite3_column_text(stmt, 6));
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL)
            cJSON_AddRawToObject(j, "tags", (const char *)sqlite3_column_text(stmt, 7));
        cJSON_AddNumberToObject(j, "score", sqlite3_column_double(stmt, 8));
        if (sqlite3_column_type(stmt, 9) != SQLITE_NULL)
            cJSON_AddStringToObject(j, "published", (const char *)sqlite3_column_text(stmt, 9));
        cJSON_AddStringToObject(j, "created_at", (const char *)sqlite3_column_text(stmt, 10));
        cJSON_AddItemToArray(arr, j);
    }
    sqlite3_finalize(stmt);
    
    char *json = cJSON_PrintUnformatted(arr);
    http_respond_json(resp, 200, json);
    cJSON_Delete(arr);
    free(json);
}

/* handler: GET /api/digest/collections */
static void handle_digest_collections(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    ensure_digest_tables();
    
    sqlite3_stmt *stmt = db_prepare(
        "SELECT id, name, description, item_ids, created_at FROM digest_collections "
        "ORDER BY created_at DESC");
    if (!stmt) { http_respond_error(resp, 500, "query failed"); return; }
    
    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *j = cJSON_CreateObject();
        cJSON_AddNumberToObject(j, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(j, "name", (const char *)sqlite3_column_text(stmt, 1));
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL)
            cJSON_AddStringToObject(j, "description", (const char *)sqlite3_column_text(stmt, 2));
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL)
            cJSON_AddRawToObject(j, "item_ids", (const char *)sqlite3_column_text(stmt, 3));
        cJSON_AddStringToObject(j, "created_at", (const char *)sqlite3_column_text(stmt, 4));
        cJSON_AddItemToArray(arr, j);
    }
    sqlite3_finalize(stmt);
    
    char *json = cJSON_PrintUnformatted(arr);
    http_respond_json(resp, 200, json);
    cJSON_Delete(arr);
    free(json);
}

/* handler: POST /api/digest/action */
static void handle_digest_action(const HttpRequest *req, HttpResponse *resp) {
    if (!req->body || !req->body_len) {
        http_respond_error(resp, 400, "empty body");
        return;
    }
    cJSON *j = cJSON_Parse(req->body);
    if (!j) { http_respond_error(resp, 400, "invalid json"); return; }
    
    cJSON *item_id = cJSON_GetObjectItem(j, "item_id");
    cJSON *action = cJSON_GetObjectItem(j, "action");
    
    if (!item_id || !cJSON_IsNumber(item_id) || !action || !cJSON_IsString(action)) {
        cJSON_Delete(j);
        http_respond_error(resp, 400, "missing item_id or action");
        return;
    }
    
    ensure_digest_tables();
    
    char created[32];
    now_str(created, sizeof(created));
    
    sqlite3_stmt *stmt = db_prepare(
        "INSERT INTO digest_user_actions (item_id, action, created_at) VALUES (?1, ?2, ?3)");
    if (!stmt) { cJSON_Delete(j); http_respond_error(resp, 500, "db prepare failed"); return; }
    
    sqlite3_bind_int(stmt, 1, (int)cJSON_GetNumberValue(item_id));
    sqlite3_bind_text(stmt, 2, cJSON_GetStringValue(action), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, created, -1, SQLITE_STATIC);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    cJSON_Delete(j);
    
    if (rc != SQLITE_DONE) {
        http_respond_error(resp, 500, "insert failed");
        return;
    }
    
    http_respond_json(resp, 201, "{\"ok\":true}");
}

void register_digest_routes(Router *r) {
    ensure_digest_tables();
    router_add(r, 'G', "/api/digest/today", handle_digest_today);
    router_add(r, 'G', "/api/digest/items", handle_digest_items);
    router_add(r, 'G', "/api/digest/collections", handle_digest_collections);
    router_add(r, 'P', "/api/digest/action", handle_digest_action);
}
