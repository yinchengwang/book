 #include "interview_api.h"
 #include "../../common/http_server.h"
 #include "../../common/db_pool.h"
 #include "cjson/cJSON.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <time.h>
 
 static int ensure_interview_tables(void) {
     const char *sql =
         "CREATE TABLE IF NOT EXISTS interview_questions ("
         "  id          TEXT PRIMARY KEY,"
         "  category    TEXT NOT NULL,"
         "  title       TEXT NOT NULL,"
         "  body        TEXT NOT NULL,"
         "  tags        TEXT NOT NULL DEFAULT '[]',"
         "  created_at  TEXT NOT NULL"
         ");"
         "CREATE TABLE IF NOT EXISTS interview_tracker ("
         "  id          TEXT PRIMARY KEY,"
         "  company     TEXT NOT NULL,"
         "  position    TEXT,"
         "  status      TEXT NOT NULL DEFAULT 'screening',"
         "  salary      TEXT,"
         "  notes       TEXT,"
         "  created_at  TEXT NOT NULL,"
         "  updated_at  TEXT NOT NULL"
         ");"
         "CREATE TABLE IF NOT EXISTS interview_rounds ("
         "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  tracker_id      TEXT NOT NULL REFERENCES interview_tracker(id),"
         "  round_type      TEXT NOT NULL,"
         "  round_date      TEXT NOT NULL,"
         "  content         TEXT,"
         "  created_at      TEXT NOT NULL"
         ");";
     if (db_exec(sql) != SQLITE_OK) {
         fprintf(stderr, "[interview_api] failed to create tables\n");
         return -1;
     }
     return 0;
 }
 
 static void now_str(char *buf, size_t len) {
     time_t t = time(NULL);
     struct tm *tm = localtime(&t);
     strftime(buf, len, "%Y-%m-%dT%H:%M:%S", tm);
 }
 
 /* ======= interview_questions ======= */
 
 static void handle_interview_questions(const HttpRequest *req, HttpResponse *resp) {
     (void)req;
     ensure_interview_tables();
 
     char sql[4096];
     snprintf(sql, sizeof(sql),
         "SELECT id, category, title, body, tags, created_at FROM interview_questions ORDER BY category, id LIMIT 5000");
 
     sqlite3_stmt *stmt = db_prepare(sql);
     if (!stmt) { http_respond_error(resp, 500, "query failed"); return; }
 
     cJSON *arr = cJSON_CreateArray();
     while (sqlite3_step(stmt) == SQLITE_ROW) {
         cJSON *j = cJSON_CreateObject();
         cJSON_AddStringToObject(j, "id", (const char *)sqlite3_column_text(stmt, 0));
         cJSON_AddStringToObject(j, "category", (const char *)sqlite3_column_text(stmt, 1));
         cJSON_AddStringToObject(j, "title", (const char *)sqlite3_column_text(stmt, 2));
         cJSON_AddStringToObject(j, "body", (const char *)sqlite3_column_text(stmt, 3));
         if (sqlite3_column_type(stmt, 4) != SQLITE_NULL)
             cJSON_AddRawToObject(j, "tags", (const char *)sqlite3_column_text(stmt, 4));
         cJSON_AddStringToObject(j, "created_at", (const char *)sqlite3_column_text(stmt, 5));
         cJSON_AddItemToArray(arr, j);
     }
     sqlite3_finalize(stmt);
 
     char *json = cJSON_PrintUnformatted(arr);
     http_respond_json(resp, 200, json);
     cJSON_Delete(arr);
     free(json);
 }
 
 /* ======= interview_tracker ======= */
 
 static void handle_tracker_list(const HttpRequest *req, HttpResponse *resp) {
     (void)req;
     ensure_interview_tables();
 
     sqlite3_stmt *stmt = db_prepare(
         "SELECT t.id, t.company, t.position, t.status, t.salary, t.notes, t.created_at, t.updated_at, "
         "COALESCE(json_group_array(json_object('id', r.id, 'round_type', r.round_type, "
         "'round_date', r.round_date, 'content', r.content, 'created_at', r.created_at)), '[]') as rounds "
         "FROM interview_tracker t LEFT JOIN interview_rounds r ON r.tracker_id = t.id "
         "GROUP BY t.id ORDER BY t.updated_at DESC");
     if (!stmt) { http_respond_error(resp, 500, "query failed"); return; }
 
     cJSON *arr = cJSON_CreateArray();
     while (sqlite3_step(stmt) == SQLITE_ROW) {
         cJSON *j = cJSON_CreateObject();
         cJSON_AddStringToObject(j, "id", (const char *)sqlite3_column_text(stmt, 0));
         cJSON_AddStringToObject(j, "company", (const char *)sqlite3_column_text(stmt, 1));
         cJSON_AddStringToObject(j, "position", (const char *)sqlite3_column_text(stmt, 2));
         cJSON_AddStringToObject(j, "status", (const char *)sqlite3_column_text(stmt, 3));
         if (sqlite3_column_type(stmt, 4) != SQLITE_NULL)
             cJSON_AddStringToObject(j, "salary", (const char *)sqlite3_column_text(stmt, 4));
         if (sqlite3_column_type(stmt, 5) != SQLITE_NULL)
             cJSON_AddStringToObject(j, "notes", (const char *)sqlite3_column_text(stmt, 5));
         cJSON_AddStringToObject(j, "created_at", (const char *)sqlite3_column_text(stmt, 6));
         cJSON_AddStringToObject(j, "updated_at", (const char *)sqlite3_column_text(stmt, 7));
         if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) {
             cJSON *rounds = cJSON_Parse((const char *)sqlite3_column_text(stmt, 8));
             if (rounds) cJSON_AddItemToObject(j, "rounds", rounds);
         }
         cJSON_AddItemToArray(arr, j);
     }
     sqlite3_finalize(stmt);
 
     char *json = cJSON_PrintUnformatted(arr);
     http_respond_json(resp, 200, json);
     cJSON_Delete(arr);
     free(json);
 }
 
 static void handle_tracker_create(const HttpRequest *req, HttpResponse *resp) {
     if (!req->body || !req->body_len) { http_respond_error(resp, 400, "empty body"); return; }
     cJSON *j = cJSON_Parse(req->body);
     if (!j) { http_respond_error(resp, 400, "invalid json"); return; }
 
     cJSON *company = cJSON_GetObjectItem(j, "company");
     cJSON *position = cJSON_GetObjectItem(j, "position");
     cJSON *status = cJSON_GetObjectItem(j, "status");
     cJSON *salary = cJSON_GetObjectItem(j, "salary");
     cJSON *notes = cJSON_GetObjectItem(j, "notes");
 
     if (!company || !cJSON_IsString(company)) {
         cJSON_Delete(j); http_respond_error(resp, 400, "missing company"); return;
     }
 
     ensure_interview_tables();
 
     char id_buf[64];
     {
         unsigned int t = (unsigned int)time(NULL);
         snprintf(id_buf, sizeof(id_buf), "tr%08x%04x", t, (t >> 16) & 0xFFFFU);
     }
     char now[32];
     now_str(now, sizeof(now));
 
     sqlite3_stmt *stmt = db_prepare(
         "INSERT INTO interview_tracker (id, company, position, status, salary, notes, created_at, updated_at) "
         "VALUES (?1,?2,?3,?4,?5,?6,?7,?8)");
     if (!stmt) { cJSON_Delete(j); http_respond_error(resp, 500, "db prepare failed"); return; }
 
     sqlite3_bind_text(stmt, 1, id_buf, -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 2, cJSON_GetStringValue(company), -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 3, position && cJSON_IsString(position) ? cJSON_GetStringValue(position) : "", -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 4, status && cJSON_IsString(status) ? cJSON_GetStringValue(status) : "screening", -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 5, salary && cJSON_IsString(salary) ? cJSON_GetStringValue(salary) : "", -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 6, notes && cJSON_IsString(notes) ? cJSON_GetStringValue(notes) : "", -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 7, now, -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 8, now, -1, SQLITE_STATIC);
 
     int rc = sqlite3_step(stmt);
     sqlite3_finalize(stmt);
     cJSON_Delete(j);
 
     if (rc != SQLITE_DONE) { http_respond_error(resp, 500, "insert failed"); return; }
 
     char json_buf[128];
     snprintf(json_buf, sizeof(json_buf), "{\"id\":\"%s\"}", id_buf);
     http_respond_json(resp, 201, json_buf);
 }
 
 /* handler: PUT /api/interview/tracker/:id */
 static void handle_tracker_update(const HttpRequest *req, HttpResponse *resp) {
     if (req->param_count < 1) { http_respond_error(resp, 400, "missing id"); return; }
     if (!req->body || !req->body_len) { http_respond_error(resp, 400, "empty body"); return; }
     cJSON *j = cJSON_Parse(req->body);
     if (!j) { http_respond_error(resp, 400, "invalid json"); return; }
 
     char updated[32];
     now_str(updated, sizeof(updated));
 
     /* 构建动态 UPDATE */
     char set_clause[1024] = "updated_at=?1";
     int param_idx = 2;
 
     const char *fields[] = {"company", "position", "status", "salary", "notes"};
     const char *col[] = {"company", "position", "status", "salary", "notes"};
     char val_buf[5][256];
     int val_used[5] = {0};
 
     for (int i = 0; i < 5; i++) {
         cJSON *v = cJSON_GetObjectItem(j, fields[i]);
         if (v && cJSON_IsString(v)) {
             char clause[64];
             snprintf(clause, sizeof(clause), ", %s=?%d", col[i], param_idx);
             strncat(set_clause, clause, sizeof(set_clause) - strlen(set_clause) - 1);
             strncpy(val_buf[i], cJSON_GetStringValue(v), sizeof(val_buf[i]) - 1);
             val_used[i] = 1;
             param_idx++;
         }
     }
     cJSON_Delete(j);
 
     char sql[2048];
     snprintf(sql, sizeof(sql), "UPDATE interview_tracker SET %s WHERE id=?%d", set_clause, param_idx);
 
     sqlite3_stmt *stmt = db_prepare(sql);
     if (!stmt) { http_respond_error(resp, 500, "db prepare failed"); return; }
 
     sqlite3_bind_text(stmt, 1, updated, -1, SQLITE_STATIC);
     int bind_idx = 2;
     for (int i = 0; i < 5; i++) {
         if (val_used[i]) {
             sqlite3_bind_text(stmt, bind_idx, val_buf[i], -1, SQLITE_STATIC);
             bind_idx++;
         }
     }
     sqlite3_bind_text(stmt, bind_idx, req->params[0], -1, SQLITE_STATIC);
 
     int rc = sqlite3_step(stmt);
     sqlite3_finalize(stmt);
 
     if (rc != SQLITE_DONE) { http_respond_error(resp, 500, "update failed"); return; }
     http_respond_json(resp, 200, "{\"ok\":true}");
 }
 
 /* handler: POST /api/interview/tracker/:id/rounds */
 static void handle_tracker_add_round(const HttpRequest *req, HttpResponse *resp) {
     if (req->param_count < 1) { http_respond_error(resp, 400, "missing tracker id"); return; }
     if (!req->body || !req->body_len) { http_respond_error(resp, 400, "empty body"); return; }
     cJSON *j = cJSON_Parse(req->body);
     if (!j) { http_respond_error(resp, 400, "invalid json"); return; }
 
     cJSON *round_type = cJSON_GetObjectItem(j, "round_type");
     cJSON *round_date = cJSON_GetObjectItem(j, "round_date");
     cJSON *content = cJSON_GetObjectItem(j, "content");
 
     if (!round_type || !cJSON_IsString(round_type) || !round_date || !cJSON_IsString(round_date)) {
         cJSON_Delete(j); http_respond_error(resp, 400, "missing round_type or round_date"); return;
     }
 
     ensure_interview_tables();
 
     char now[32];
     now_str(now, sizeof(now));
 
     sqlite3_stmt *stmt = db_prepare(
         "INSERT INTO interview_rounds (tracker_id, round_type, round_date, content, created_at) "
         "VALUES (?1,?2,?3,?4,?5)");
     if (!stmt) { cJSON_Delete(j); http_respond_error(resp, 500, "db prepare failed"); return; }
 
     sqlite3_bind_text(stmt, 1, req->params[0], -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 2, cJSON_GetStringValue(round_type), -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 3, cJSON_GetStringValue(round_date), -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 4, content && cJSON_IsString(content) ? cJSON_GetStringValue(content) : "", -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 5, now, -1, SQLITE_STATIC);
 
     int rc = sqlite3_step(stmt);
     sqlite3_finalize(stmt);
     cJSON_Delete(j);
 
     if (rc != SQLITE_DONE) { http_respond_error(resp, 500, "insert failed"); return; }
 
     /* 更新 tracker updated_at */
     sqlite3_stmt *upd = db_prepare("UPDATE interview_tracker SET updated_at=?1 WHERE id=?2");
     if (upd) {
         sqlite3_bind_text(upd, 1, now, -1, SQLITE_STATIC);
         sqlite3_bind_text(upd, 2, req->params[0], -1, SQLITE_STATIC);
         sqlite3_step(upd);
         sqlite3_finalize(upd);
     }
 
     http_respond_json(resp, 201, "{\"ok\":true}");
 }
 
 /* handler: GET /api/interview/tracker (no-id: list all) */
 void register_interview_routes(Router *r) {
     ensure_interview_tables();
     router_add(r, 'G', "/api/interview/questions", handle_interview_questions);
     router_add(r, 'G', "/api/interview/tracker", handle_tracker_list);
     router_add(r, 'P', "/api/interview/tracker", handle_tracker_create);
     router_add(r, 'U', "/api/interview/tracker/:id", handle_tracker_update);
     router_add(r, 'P', "/api/interview/tracker/:id/rounds", handle_tracker_add_round);
 }
