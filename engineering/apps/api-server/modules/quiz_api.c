 #include "quiz_api.h"
 #include "../../common/http_server.h"
 #include "../../common/db_pool.h"
 #include "cjson/cJSON.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <time.h>
 
 /* 纭繚 quiz 琛ㄥ瓨鍦?*/
 static int ensure_quiz_tables(void) {
     const char *sql =
         "CREATE TABLE IF NOT EXISTS quiz_questions ("
         "  id          TEXT PRIMARY KEY,"
         "  stack       TEXT NOT NULL,"
         "  title       TEXT NOT NULL,"
         "  category    TEXT NOT NULL DEFAULT '',"
         "  difficulty  TEXT NOT NULL DEFAULT '',"
         "  options     TEXT,"
         "  answer      TEXT NOT NULL,"
         "  explanation TEXT,"
         "  tags        TEXT NOT NULL DEFAULT '[]',"
         "  time_estimate INTEGER DEFAULT 0,"
         "  created_at  TEXT NOT NULL"
         ");"
         "CREATE TABLE IF NOT EXISTS quiz_answers ("
         "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
         "  question_id TEXT NOT NULL REFERENCES quiz_questions(id),"
         "  user_answer TEXT NOT NULL,"
         "  correct     INTEGER NOT NULL DEFAULT 0,"
         "  date        TEXT NOT NULL,"
         "  timestamp   INTEGER NOT NULL,"
         "  time_spent  INTEGER DEFAULT 0"
         ");";
     if (db_exec(sql) != SQLITE_OK) {
         fprintf(stderr, "[quiz_api] failed to create tables\n");
         return -1;
     }
     return 0;
 }
 
 /* handler: GET /api/quiz/questions?stack=&category=&difficulty=&page=&per_page= */
static void handle_quiz_questions(const HttpRequest *req, HttpResponse *resp) {
    if (ensure_quiz_tables() != 0) {
        http_respond_error(resp, 500, "db init failed");
        return;
    }
    
    char where[2048] = "WHERE 1=1";
    
    /* Parse pagination from query_string */
    const char *qs = req->query_string;
    
    int page = 0, per_page = 50;
    const char *pp = strstr(qs, "per_page=");
    if (pp) { pp += 9; per_page = atoi(pp); }
    const char *pg = strstr(qs, "page=");
    if (pg) { pg += 5; page = atoi(pg); }
    if (per_page < 1) per_page = 50;
    if (per_page > 200) per_page = 200;
    int limit = per_page;
    int offset = page * per_page;
    
    char count_sql[4096];
    snprintf(count_sql, sizeof(count_sql),
        "SELECT count(*) FROM quiz_questions %s", where);
    
    int total = 0;
    sqlite3_stmt *cnt = db_prepare(count_sql);
    if (cnt) {
        if (sqlite3_step(cnt) == SQLITE_ROW)
            total = sqlite3_column_int(cnt, 0);
        sqlite3_finalize(cnt);
    }
    
    char sql[8192];
    snprintf(sql, sizeof(sql),
        "SELECT id, stack, title, category, difficulty, options, answer, "
        "explanation, tags, time_estimate, created_at FROM quiz_questions %s "
        "ORDER BY stack, created_at LIMIT %d OFFSET %d", where, limit, offset);
    
    sqlite3_stmt *stmt = db_prepare(sql);
    if (!stmt) {
        http_respond_error(resp, 500, "query failed");
        return;
    }
    
    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *j = cJSON_CreateObject();
        cJSON_AddStringToObject(j, "id", (const char *)sqlite3_column_text(stmt, 0));
        cJSON_AddStringToObject(j, "stack", (const char *)sqlite3_column_text(stmt, 1));
        cJSON_AddStringToObject(j, "title", (const char *)sqlite3_column_text(stmt, 2));
        cJSON_AddStringToObject(j, "category", (const char *)sqlite3_column_text(stmt, 3));
        cJSON_AddStringToObject(j, "difficulty", (const char *)sqlite3_column_text(stmt, 4));
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL)
            cJSON_AddRawToObject(j, "options", (const char *)sqlite3_column_text(stmt, 5));
        cJSON_AddStringToObject(j, "answer", (const char *)sqlite3_column_text(stmt, 6));
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL)
            cJSON_AddStringToObject(j, "explanation", (const char *)sqlite3_column_text(stmt, 7));
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL)
            cJSON_AddRawToObject(j, "tags", (const char *)sqlite3_column_text(stmt, 8));
        cJSON_AddNumberToObject(j, "time_estimate", sqlite3_column_int(stmt, 9));
        cJSON_AddStringToObject(j, "created_at", (const char *)sqlite3_column_text(stmt, 10));
        cJSON_AddItemToArray(arr, j);
    }
    sqlite3_finalize(stmt);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "questions", arr);
    cJSON_AddNumberToObject(root, "total", total);
    char *json = cJSON_PrintUnformatted(root);
    http_respond_json(resp, 200, json);
    cJSON_Delete(root);
    free(json);
}

/* handler: POST /api/quiz/answers */
 static void handle_quiz_submit_answer(const HttpRequest *req, HttpResponse *resp) {
     if (!req->body || !req->body_len) {
         http_respond_error(resp, 400, "empty body");
         return;
     }
     cJSON *j = cJSON_Parse(req->body);
     if (!j) {
         http_respond_error(resp, 400, "invalid json");
         return;
     }
 
     cJSON *qid = cJSON_GetObjectItem(j, "question_id");
     cJSON *ans = cJSON_GetObjectItem(j, "user_answer");
     cJSON *correct = cJSON_GetObjectItem(j, "correct");
     cJSON *time_spent = cJSON_GetObjectItem(j, "time_spent");
 
     if (!qid || !cJSON_IsString(qid) || !ans || !cJSON_IsString(ans)) {
         cJSON_Delete(j);
         http_respond_error(resp, 400, "missing question_id or user_answer");
         return;
     }
 
     ensure_quiz_tables();
 
     time_t now = time(NULL);
     char date_buf[16];
     struct tm *tm = localtime(&now);
     strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm);
 
     sqlite3_stmt *stmt = db_prepare(
         "INSERT INTO quiz_answers (question_id, user_answer, correct, date, timestamp, time_spent) "
         "VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
     if (!stmt) {
         cJSON_Delete(j);
         http_respond_error(resp, 500, "db prepare failed");
         return;
     }
 
     sqlite3_bind_text(stmt, 1, cJSON_GetStringValue(qid), -1, SQLITE_STATIC);
     sqlite3_bind_text(stmt, 2, cJSON_GetStringValue(ans), -1, SQLITE_STATIC);
     sqlite3_bind_int(stmt, 3, correct && cJSON_IsTrue(correct) ? 1 : 0);
     sqlite3_bind_text(stmt, 4, date_buf, -1, SQLITE_STATIC);
     sqlite3_bind_int64(stmt, 5, (long long)now);
     sqlite3_bind_int(stmt, 6, time_spent && cJSON_IsNumber(time_spent) ? (int)cJSON_GetNumberValue(time_spent) : 0);
 
     int rc = sqlite3_step(stmt);
     sqlite3_finalize(stmt);
     cJSON_Delete(j);
 
     if (rc != SQLITE_DONE) {
         http_respond_error(resp, 500, "insert failed");
         return;
     }
 
     http_respond_json(resp, 201, "{\"ok\":true}");
 }
 
 /* handler: GET /api/quiz/stats */
 static void handle_quiz_stats(const HttpRequest *req, HttpResponse *resp) {
     (void)req;
     ensure_quiz_tables();
 
     /* 鎸?stack 缁熻 */
     cJSON *stacks = cJSON_CreateObject();
     sqlite3_stmt *stmt = db_prepare(
         "SELECT q.stack, COUNT(DISTINCT q.id), "
         "COUNT(a.id), SUM(CASE WHEN a.correct=1 THEN 1 ELSE 0 END) "
         "FROM quiz_questions q LEFT JOIN quiz_answers a ON q.id = a.question_id "
         "GROUP BY q.stack");
     if (stmt) {
         while (sqlite3_step(stmt) == SQLITE_ROW) {
             const char *stack = (const char *)sqlite3_column_text(stmt, 0);
             int total = sqlite3_column_int(stmt, 1);
             int answered = sqlite3_column_int(stmt, 2);
             int correct = sqlite3_column_int(stmt, 3);
             cJSON *s = cJSON_CreateObject();
             cJSON_AddNumberToObject(s, "total", total);
             cJSON_AddNumberToObject(s, "answered", answered);
             cJSON_AddNumberToObject(s, "correct", correct);
             cJSON_AddItemToObject(stacks, stack ? stack : "unknown", s);
         }
         sqlite3_finalize(stmt);
     }
 
     cJSON *root = cJSON_CreateObject();
     cJSON_AddItemToObject(root, "stacks", stacks);
     cJSON_AddNumberToObject(root, "total_questions", 0);
     char *json = cJSON_PrintUnformatted(root);
     http_respond_json(resp, 200, json);
     cJSON_Delete(root);
     free(json);
 }
 
 void register_quiz_routes(Router *r) {
     ensure_quiz_tables();
     router_add(r, 'G', "/api/quiz/questions", handle_quiz_questions);
     router_add(r, 'P', "/api/quiz/answers", handle_quiz_submit_answer);
     router_add(r, 'G', "/api/quiz/stats", handle_quiz_stats);
 }


