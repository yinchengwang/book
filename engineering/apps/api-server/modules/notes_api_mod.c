#include "notes_api_mod.h"
#include "../../common/http_server.h"
#include "../../common/db_pool.h"
#include "../../common/notes_api.h"
#include "../config.h"
#include "cjson/cJSON.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *g_notes_root = NULL;

/* handler: GET /api/notes?context=&parent_dir=&page=&per_page= */
static void handle_notes_list(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    NoteMeta *notes = (NoteMeta *)calloc(128, sizeof(NoteMeta));
    int count = 0, total = 0;
    notes_list(NULL, NULL, 0, 0, notes, &count, &total);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *j = cJSON_CreateObject();
        cJSON_AddStringToObject(j, "id", notes[i].id);
        cJSON_AddStringToObject(j, "file_path", notes[i].file_path);
        cJSON_AddStringToObject(j, "title", notes[i].title);
        cJSON_AddStringToObject(j, "tags", notes[i].tags);
        cJSON_AddStringToObject(j, "context", notes[i].context);
        cJSON_AddStringToObject(j, "source", notes[i].source);
        cJSON_AddStringToObject(j, "status", notes[i].status);
        cJSON_AddNumberToObject(j, "rating", notes[i].rating);
        cJSON_AddNumberToObject(j, "favorite", notes[i].favorite);
        cJSON_AddStringToObject(j, "created_at", notes[i].created_at);
        cJSON_AddStringToObject(j, "updated_at", notes[i].updated_at);
        cJSON_AddItemToArray(arr, j);
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "notes", arr);
    cJSON_AddNumberToObject(root, "total", total);

    char *json = cJSON_PrintUnformatted(root);
    http_respond_json(resp, 200, json);
    cJSON_Delete(root);
    free(json);
    free(notes);
}

/* handler: GET /api/notes/:id */
static void handle_notes_get(const HttpRequest *req, HttpResponse *resp) {
    if (req->param_count < 1) { http_respond_error(resp, 400, "missing id"); return; }
    NoteMeta note;
    if (notes_get_by_id(req->params[0], &note) != 0) {
        http_respond_error(resp, 404, "not found");
        return;
    }
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "id", note.id);
    cJSON_AddStringToObject(j, "file_path", note.file_path);
    cJSON_AddStringToObject(j, "title", note.title);
    cJSON_AddStringToObject(j, "body", note.body);
    cJSON_AddStringToObject(j, "tags", note.tags);
    cJSON_AddStringToObject(j, "context", note.context);
    cJSON_AddStringToObject(j, "source", note.source);
    cJSON_AddStringToObject(j, "status", note.status);
    cJSON_AddNumberToObject(j, "rating", note.rating);
    cJSON_AddNumberToObject(j, "favorite", note.favorite);
    cJSON_AddStringToObject(j, "created_at", note.created_at);
    cJSON_AddStringToObject(j, "updated_at", note.updated_at);
    char *json = cJSON_PrintUnformatted(j);
    http_respond_json(resp, 200, json);
    cJSON_Delete(j);
    free(json);
}

/* handler: POST /api/notes */
static void handle_notes_create(const HttpRequest *req, HttpResponse *resp) {
    fprintf(stderr, "[notes] handle_notes_create body_len=%zu body=%.50s\n", 
        req->body_len, req->body ? req->body : "(null)");
    if (!req->body || !req->body_len) { http_respond_error(resp, 400, "empty body"); return; }
    cJSON *j = cJSON_Parse(req->body);
    if (!j) { http_respond_error(resp, 400, "invalid json"); return; }

    char note_id[64] = "";
    /* 生成 id */
    {
        unsigned int _t = (unsigned int)time(NULL);
        snprintf(note_id, sizeof(note_id), "n%08x%04x", _t, (_t >> 16) & 0xFFFFU);
    }

    NoteMeta note;
    memset(&note, 0, sizeof(note));
    strncpy(note.id, note_id, sizeof(note.id) - 1);
    cJSON *v;

    v = cJSON_GetObjectItem(j, "file_path");
    if (v && cJSON_IsString(v)) strncpy(note.file_path, cJSON_GetStringValue(v), sizeof(note.file_path) - 1);
    v = cJSON_GetObjectItem(j, "title");
    if (v && cJSON_IsString(v)) strncpy(note.title, cJSON_GetStringValue(v), sizeof(note.title) - 1);
    v = cJSON_GetObjectItem(j, "body");
    if (v && cJSON_IsString(v)) strncpy(note.body, cJSON_GetStringValue(v), sizeof(note.body) - 1);
    v = cJSON_GetObjectItem(j, "context");
    if (v && cJSON_IsString(v)) strncpy(note.context, cJSON_GetStringValue(v), sizeof(note.context) - 1);
    v = cJSON_GetObjectItem(j, "source");
    if (v && cJSON_IsString(v)) strncpy(note.source, cJSON_GetStringValue(v), sizeof(note.source) - 1);
    v = cJSON_GetObjectItem(j, "tags");
    if (v && cJSON_IsString(v)) strncpy(note.tags, cJSON_GetStringValue(v), sizeof(note.tags) - 1);
    cJSON_Delete(j);

    if (notes_create(g_notes_root, &note) != 0) {
        http_respond_error(resp, 500, "create failed");
        return;
    }
    char json_buf[256];
    snprintf(json_buf, sizeof(json_buf), "{\"id\":\"%s\"}", note_id);
    http_respond_json(resp, 201, json_buf);
}

/* handler: GET /api/notes/tree */
static void handle_notes_tree(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    static char tree_json[65536];
    notes_get_tree(tree_json, sizeof(tree_json));
    http_respond_json(resp, 200, tree_json);
}

/* handler: POST /api/notes/dir */
static void handle_notes_create_dir(const HttpRequest *req, HttpResponse *resp) {
    if (!req->body || !req->body_len) { http_respond_error(resp, 400, "empty body"); return; }
    cJSON *j = cJSON_Parse(req->body);
    if (!j) { http_respond_error(resp, 400, "invalid json"); return; }
    cJSON *v = cJSON_GetObjectItem(j, "path");
    if (!v || !cJSON_IsString(v)) {
        cJSON_Delete(j);
        http_respond_error(resp, 400, "missing path");
        return;
    }
    const char *path = cJSON_GetStringValue(v);
    cJSON_Delete(j);

    if (notes_create_dir(g_notes_root, path) != 0) {
        http_respond_error(resp, 500, "create dir failed");
        return;
    }
    http_respond_json(resp, 201, "{\"ok\":true}");
}

/* handler: PUT /api/notes/:id */
static void handle_notes_update(const HttpRequest *req, HttpResponse *resp) {
    if (req->param_count < 1) { http_respond_error(resp, 400, "missing id"); return; }
    if (!req->body || !req->body_len) { http_respond_error(resp, 400, "empty body"); return; }
    cJSON *j = cJSON_Parse(req->body);
    if (!j) { http_respond_error(resp, 400, "invalid json"); return; }
    NoteMeta note;
    memset(&note, 0, sizeof(note));
    strncpy(note.id, req->params[0], sizeof(note.id) - 1);
    cJSON *v;
    v = cJSON_GetObjectItem(j, "title");
    if (v && cJSON_IsString(v)) strncpy(note.title, cJSON_GetStringValue(v), sizeof(note.title) - 1);
    v = cJSON_GetObjectItem(j, "body");
    if (v && cJSON_IsString(v)) strncpy(note.body, cJSON_GetStringValue(v), sizeof(note.body) - 1);
    v = cJSON_GetObjectItem(j, "tags");
    if (v && cJSON_IsString(v)) strncpy(note.tags, cJSON_GetStringValue(v), sizeof(note.tags) - 1);
    v = cJSON_GetObjectItem(j, "source");
    if (v && cJSON_IsString(v)) strncpy(note.source, cJSON_GetStringValue(v), sizeof(note.source) - 1);
    v = cJSON_GetObjectItem(j, "status");
    if (v && cJSON_IsString(v)) strncpy(note.status, cJSON_GetStringValue(v), sizeof(note.status) - 1);
    v = cJSON_GetObjectItem(j, "rating");
    if (v && cJSON_IsNumber(v)) note.rating = (int)cJSON_GetNumberValue(v);
    v = cJSON_GetObjectItem(j, "favorite");
    if (v && cJSON_IsNumber(v)) note.favorite = (int)cJSON_GetNumberValue(v);
    cJSON_Delete(j);
    if (notes_update(g_notes_root, &note) != 0) {
        http_respond_error(resp, 500, "update failed");
        return;
    }
    http_respond_json(resp, 200, "{\"ok\":true}");
}

/* handler: DELETE /api/notes/:id */
static void handle_notes_delete(const HttpRequest *req, HttpResponse *resp) {
    if (req->param_count < 1) { http_respond_error(resp, 400, "missing id"); return; }
    if (notes_delete(g_notes_root, req->params[0]) != 0) {
        http_respond_error(resp, 500, "delete failed");
        return;
    }
    http_respond_json(resp, 200, "{\"ok\":true}");
}

/* handler: GET /api/notes/search?q= */
static void handle_notes_search(const HttpRequest *req, HttpResponse *resp) {
    const char *qs = req->query_string;
    const char *qpos = strstr(qs, "q=");
    if (!qpos) { http_respond_json(resp, 200, "{\"notes\":[],\"total\":0}"); return; }
    qpos += 2;
    char query[256];
    strncpy(query, qpos, sizeof(query) - 1);
    for (char *p = query; *p; p++) { if (*p == '+') *p = ' '; }
    NoteMeta results[100];
    int count = 0;
    notes_search(query, results, &count, 100);
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *j = cJSON_CreateObject();
        cJSON_AddStringToObject(j, "id", results[i].id);
        cJSON_AddStringToObject(j, "title", results[i].title);
        cJSON_AddStringToObject(j, "body", results[i].body);
        cJSON_AddStringToObject(j, "tags", results[i].tags);
        cJSON_AddStringToObject(j, "context", results[i].context);
        cJSON_AddItemToArray(arr, j);
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "notes", arr);
    cJSON_AddNumberToObject(root, "total", count);
    char *json = cJSON_PrintUnformatted(root);
    http_respond_json(resp, 200, json);
    cJSON_Delete(root);
    free(json);
}

void register_notes_routes(Router *r, const char *notes_root) {
    g_notes_root = notes_root;
    notes_init_db();
    router_add(r, 'G', "/api/notes", handle_notes_list);
    router_add(r, 'G', "/api/notes/tree", handle_notes_tree);
    router_add(r, 'P', "/api/notes/dir", handle_notes_create_dir);
    router_add(r, 'G', "/api/notes/:id", handle_notes_get);
    router_add(r, 'P', "/api/notes", handle_notes_create);
    router_add(r, 'U', "/api/notes/:id", handle_notes_update);
    router_add(r, 'D', "/api/notes/:id", handle_notes_delete);
    router_add(r, 'G', "/api/notes/search", handle_notes_search);
}






