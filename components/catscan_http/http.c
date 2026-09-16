#include "catscan_http.h"
#include "esp_http_server.h"
#include <string.h>
static catscan_snapshot_fn take_snapshot;
static catscan_configure_fn apply_config;
extern const char index_start[] asm("_binary_index_html_start");
static esp_err_t send_json(httpd_req_t *req, cJSON *json) {
    if (!json) return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"out of memory");
    char *body=cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!body) return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"out of memory");
    httpd_resp_set_type(req,"application/json");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_send(req,body,HTTPD_RESP_USE_STRLEN);
    cJSON_free(body); return err;
}
static esp_err_t get_state(httpd_req_t *req) {
    catscan_state_t s; uint64_t now; take_snapshot(&s,&now);
    return send_json(req,catscan_state_json(&s,now));
}
static esp_err_t get_config(httpd_req_t *req) {
    catscan_state_t s; uint64_t now; take_snapshot(&s,&now);
    return send_json(req,catscan_config_json(&s.config));
}
static esp_err_t put_config(httpd_req_t *req) {
    if (!req->content_len || req->content_len>256)
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"body must be 1..256 bytes");
    char type[64];
    if (httpd_req_get_hdr_value_str(req,"Content-Type",type,sizeof(type))!=ESP_OK ||
        (strcmp(type,"application/json")!=0 && strcmp(type,"application/json; charset=utf-8")!=0))
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Content-Type must be application/json");
    char body[257]; size_t used=0;
    while (used<req->content_len) {
        int n=httpd_req_recv(req,body+used,req->content_len-used);
        if (n<=0) {
            httpd_resp_send_err(req,HTTPD_408_REQ_TIMEOUT,"incomplete request");
            return ESP_FAIL; /* close socket; unread body must not become another request */
        }
        used+=(size_t)n;
    }
    body[used]=0;
    catscan_config_t c;
    if (!catscan_config_parse(body,used,&c))
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"invalid config: require sample_interval_ms and gain only");
    if (!apply_config(c)) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"unsupported config");
    return get_config(req);
}
static esp_err_t get_index(httpd_req_t *req) {
    httpd_resp_set_type(req,"text/html");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_send(req,index_start,HTTPD_RESP_USE_STRLEN);
}
esp_err_t catscan_http_start(catscan_snapshot_fn snapshot, catscan_configure_fn configure) {
    if (!snapshot || !configure) return ESP_ERR_INVALID_ARG;
    take_snapshot=snapshot; apply_config=configure;
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();
    cfg.stack_size=6144; cfg.recv_wait_timeout=2; cfg.send_wait_timeout=2;
    httpd_handle_t server=NULL;
    esp_err_t err=httpd_start(&server,&cfg);
    if (err!=ESP_OK) return err;
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=get_index},
        {.uri="/api/state",.method=HTTP_GET,.handler=get_state},
        {.uri="/api/config",.method=HTTP_GET,.handler=get_config},
        {.uri="/api/config",.method=HTTP_PUT,.handler=put_config}
    };
    for (unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i) {
        err=httpd_register_uri_handler(server,&routes[i]);
        if (err!=ESP_OK) { httpd_stop(server); return err; }
    }
    return ESP_OK;
}
