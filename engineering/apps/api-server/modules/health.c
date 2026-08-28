#include "health.h"
#include "../../common/http_server.h"
#include "../../common/db_pool.h"
#include "cjson/cJSON.h"

static void handle_health(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddBoolToObject(root, "db_open", db_handle() != NULL);
    char *json = cJSON_PrintUnformatted(root);
    http_respond_json(resp, 200, json);
    cJSON_Delete(root);
}

void register_health_routes(Router *r) {
    router_add(r, 'G', "/api/health", handle_health);
}
