#include "review_api.h"
#include "../../common/http_server.h"
#include "../../common/db_pool.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

static int ensure_review_tables(void) {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS review_records ("
        "  id TEXT PRIMARY KEY,"
        "  question TEXT NOT NULL,"
        "  answer TEXT NOT NULL,"
        "  domain TEXT NOT NULL DEFAULT '',"
        "  easiness REAL NOT NULL DEFAULT 2.5,"
        "  interval INTEGER NOT NULL DEFAULT 1,"
        "  repetitions INTEGER NOT NULL DEFAULT 0,"
        "  next_review_date TEXT NOT NULL,"
        "  last_review_date TEXT NOT NULL DEFAULT '',"
        "  created_at TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS review_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  record_id TEXT NOT NULL,"
        "  quality INTEGER NOT NULL,"
        "  reviewed_at TEXT NOT NULL"
        ");";
    if (db_exec(sql) != SQLITE_OK) return -1;
    return 0;
}

static void now_str(char *b, size_t l) {
    time_t t = time(NULL);
    strftime(b, l, "%Y-%m-%dT%H:%M:%S", localtime(&t));
}

static void sm2_calc(double ef, int iv, int rp, int q, double *oef, int *oiv, int *orp) {
    *oef = ef + (0.1 - (5 - q) * (0.08 + (5 - q) * 0.02));
    if (*oef < 1.3) *oef = 1.3;
    if (q < 3) { *orp = 0; *oiv = 1; }
    else { *orp = rp + 1; if (*orp == 1) *oiv = 1; else if (*orp == 2) *oiv = 6; else *oiv = (int)(iv * *oef + 0.5); }
}

static void handle_review_due(const HttpRequest *req, HttpResponse *resp) {
    (void)req; ensure_review_tables();
    char now[32]; now_str(now, sizeof(now));
    sqlite3_stmt *s = db_prepare(
        "SELECT id,question,answer,domain,easiness,interval,repetitions,"
        "next_review_date,last_review_date,created_at FROM review_records "
        "WHERE next_review_date <= ?1 ORDER BY next_review_date ASC LIMIT 100");
    if (!s) { http_respond_error(resp, 500, "query failed"); return; }
    sqlite3_bind_text(s, 1, now, -1, SQLITE_STATIC);
    cJSON *a = cJSON_CreateArray();
    while (sqlite3_step(s) == SQLITE_ROW) {
        cJSON *j = cJSON_CreateObject();
        cJSON_AddStringToObject(j, "id", (const char*)sqlite3_column_text(s,0));
        cJSON_AddStringToObject(j, "question", (const char*)sqlite3_column_text(s,1));
        cJSON_AddStringToObject(j, "answer", (const char*)sqlite3_column_text(s,2));
        cJSON_AddStringToObject(j, "domain", (const char*)sqlite3_column_text(s,3));
        cJSON_AddNumberToObject(j, "easiness", sqlite3_column_double(s,4));
        cJSON_AddNumberToObject(j, "interval", sqlite3_column_int(s,5));
        cJSON_AddNumberToObject(j, "repetitions", sqlite3_column_int(s,6));
        cJSON_AddStringToObject(j, "next_review_date", (const char*)sqlite3_column_text(s,7));
        cJSON_AddStringToObject(j, "last_review_date", (const char*)sqlite3_column_text(s,8));
        cJSON_AddStringToObject(j, "created_at", (const char*)sqlite3_column_text(s,9));
        cJSON_AddItemToArray(a, j);
    }
    sqlite3_finalize(s);
    cJSON *r = cJSON_CreateObject(); cJSON_AddItemToObject(r, "records", a);
    char *j = cJSON_PrintUnformatted(r);
    http_respond_json(resp, 200, j); cJSON_Delete(r); free(j);
}

static void handle_review_rate(const HttpRequest *req, HttpResponse *resp) {
    if (!req->body||!req->body_len) { http_respond_error(resp,400,"empty body"); return; }
    cJSON *j = cJSON_Parse(req->body);
    if (!j) { http_respond_error(resp,400,"invalid json"); return; }
    cJSON *rid = cJSON_GetObjectItem(j,"record_id");
    cJSON *qj = cJSON_GetObjectItem(j,"quality");
    if (!rid||!cJSON_IsString(rid)||!qj||!cJSON_IsNumber(qj)) { cJSON_Delete(j); http_respond_error(resp,400,"missing"); return; }
    int q = (int)cJSON_GetNumberValue(qj);
    if (q<0||q>5) { cJSON_Delete(j); http_respond_error(resp,400,"0-5"); return; }
    ensure_review_tables();
    sqlite3_stmt *s = db_prepare("SELECT easiness,interval,repetitions FROM review_records WHERE id=?1");
    if (!s) { cJSON_Delete(j); http_respond_error(resp,500,"db"); return; }
    sqlite3_bind_text(s,1,cJSON_GetStringValue(rid),-1,SQLITE_STATIC);
    if (sqlite3_step(s)!=SQLITE_ROW) { sqlite3_finalize(s); cJSON_Delete(j); http_respond_error(resp,404,"not found"); return; }
    double ef=sqlite3_column_double(s,0); int iv=sqlite3_column_int(s,1), rp=sqlite3_column_int(s,2);
    sqlite3_finalize(s);
    double nef; int niv, nrp; sm2_calc(ef,iv,rp,q,&nef,&niv,&nrp);
    char now[32], nd[32]; now_str(now,sizeof(now));
    time_t t=time(NULL)+niv*86400; strftime(nd,sizeof(nd),"%Y-%m-%dT%H:%M:%S",localtime(&t));
    sqlite3_stmt *u = db_prepare("UPDATE review_records SET easiness=?1,interval=?2,repetitions=?3,next_review_date=?4,last_review_date=?5 WHERE id=?6");
    if (!u) { cJSON_Delete(j); http_respond_error(resp,500,"db"); return; }
    sqlite3_bind_double(u,1,nef); sqlite3_bind_int(u,2,niv); sqlite3_bind_int(u,3,nrp);
    sqlite3_bind_text(u,4,nd,-1,SQLITE_STATIC); sqlite3_bind_text(u,5,now,-1,SQLITE_STATIC);
    sqlite3_bind_text(u,6,cJSON_GetStringValue(rid),-1,SQLITE_STATIC);
    sqlite3_step(u); sqlite3_finalize(u);
    cJSON_Delete(j);
    char buf[256]; snprintf(buf,sizeof(buf),"{\"ok\":true,\"next_review_date\":\"%s\",\"interval\":%d}",nd,niv);
    http_respond_json(resp,200,buf);
}

static void handle_review_stats(const HttpRequest *req, HttpResponse *resp) {
    (void)req; ensure_review_tables();
    int total=0,due=0,reviewed=0; sqlite3_stmt *s;
    s=db_prepare("SELECT COUNT(*) FROM review_records");
    if(s&&sqlite3_step(s)==SQLITE_ROW)total=sqlite3_column_int(s,0); sqlite3_finalize(s);
    s=db_prepare("SELECT COUNT(*) FROM review_records WHERE next_review_date <= datetime('now','localtime')");
    if(s&&sqlite3_step(s)==SQLITE_ROW)due=sqlite3_column_int(s,0); sqlite3_finalize(s);
    s=db_prepare("SELECT COUNT(*) FROM review_log");
    if(s&&sqlite3_step(s)==SQLITE_ROW)reviewed=sqlite3_column_int(s,0); sqlite3_finalize(s);
    cJSON *r=cJSON_CreateObject(); cJSON_AddNumberToObject(r,"total",total);
    cJSON_AddNumberToObject(r,"due",due); cJSON_AddNumberToObject(r,"total_reviews",reviewed);
    char *j=cJSON_PrintUnformatted(r); http_respond_json(resp,200,j); cJSON_Delete(r); free(j);
}

static void handle_review_records_list(const HttpRequest *req, HttpResponse *resp) {
    (void)req; ensure_review_tables();
    sqlite3_stmt *s=db_prepare("SELECT id,question,answer,domain,easiness,interval,repetitions,next_review_date,last_review_date,created_at FROM review_records ORDER BY next_review_date ASC LIMIT 500");
    if(!s){http_respond_error(resp,500,"query");return;}
    cJSON *a=cJSON_CreateArray();
    while(sqlite3_step(s)==SQLITE_ROW){
        cJSON*j=cJSON_CreateObject();cJSON_AddStringToObject(j,"id",(const char*)sqlite3_column_text(s,0));
        cJSON_AddStringToObject(j,"question",(const char*)sqlite3_column_text(s,1));
        cJSON_AddStringToObject(j,"answer",(const char*)sqlite3_column_text(s,2));
        cJSON_AddStringToObject(j,"domain",(const char*)sqlite3_column_text(s,3));
        cJSON_AddNumberToObject(j,"easiness",sqlite3_column_double(s,4));
        cJSON_AddNumberToObject(j,"interval",sqlite3_column_int(s,5));
        cJSON_AddNumberToObject(j,"repetitions",sqlite3_column_int(s,6));
        cJSON_AddStringToObject(j,"next_review_date",(const char*)sqlite3_column_text(s,7));
        cJSON_AddStringToObject(j,"last_review_date",(const char*)sqlite3_column_text(s,8));
        cJSON_AddStringToObject(j,"created_at",(const char*)sqlite3_column_text(s,9));
        cJSON_AddItemToArray(a,j);
    }sqlite3_finalize(s);
    char*j=cJSON_PrintUnformatted(a);http_respond_json(resp,200,j);cJSON_Delete(a);free(j);
}

static void handle_review_records_create(const HttpRequest *req, HttpResponse *resp) {
    if(!req->body||!req->body_len){http_respond_error(resp,400,"empty");return;}
    cJSON*j=cJSON_Parse(req->body);if(!j){http_respond_error(resp,400,"json");return;}
    cJSON*q=cJSON_GetObjectItem(j,"question"),*a=cJSON_GetObjectItem(j,"answer"),*d=cJSON_GetObjectItem(j,"domain");
    if(!q||!cJSON_IsString(q)||!a||!cJSON_IsString(a)){cJSON_Delete(j);http_respond_error(resp,400,"missing");return;}
    ensure_review_tables();
    char id[64],now[32];
    snprintf(id,sizeof(id),"rev%08x%04x",(unsigned)time(NULL),((unsigned)time(NULL)>>16)&0xFFFFU);
    now_str(now,sizeof(now));
    sqlite3_stmt*s=db_prepare("INSERT INTO review_records(id,question,answer,domain,easiness,interval,repetitions,next_review_date,last_review_date,created_at)VALUES(?1,?2,?3,?4,2.5,1,0,?5,'',?5)");
    if(!s){cJSON_Delete(j);http_respond_error(resp,500,"db");return;}
    sqlite3_bind_text(s,1,id,-1,SQLITE_STATIC);sqlite3_bind_text(s,2,cJSON_GetStringValue(q),-1,SQLITE_STATIC);
    sqlite3_bind_text(s,3,cJSON_GetStringValue(a),-1,SQLITE_STATIC);
    sqlite3_bind_text(s,4,d&&cJSON_IsString(d)?cJSON_GetStringValue(d):"",-1,SQLITE_STATIC);
    sqlite3_bind_text(s,5,now,-1,SQLITE_STATIC);
    int rc=sqlite3_step(s);sqlite3_finalize(s);cJSON_Delete(j);
    if(rc!=SQLITE_DONE){http_respond_error(resp,500,"insert");return;}
    char buf[128];snprintf(buf,sizeof(buf),"{\"id\":\"%s\"}",id);http_respond_json(resp,201,buf);
}

void register_review_routes(Router *r) {
    ensure_review_tables();
    router_add(r,'G',"/api/review/due",handle_review_due);
    router_add(r,'P',"/api/review/rate",handle_review_rate);
    router_add(r,'G',"/api/review/stats",handle_review_stats);
    router_add(r,'G',"/api/review/records",handle_review_records_list);
    router_add(r,'P',"/api/review/records",handle_review_records_create);
}