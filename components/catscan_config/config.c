#include "catscan_config.h"
#include <math.h>
#include <string.h>
catscan_config_t catscan_config_default(void) { return (catscan_config_t){1000, 1}; }
bool catscan_config_valid(const catscan_config_t *c) {
    static const uint32_t intervals[] = {100,250,500,1000,2000,5000,10000};
    if (!c || !catscan_gain_valid(c->gain) || c->sample_interval_ms < CATSCAN_SCAN_BUDGET_MS) return false;
    for (unsigned i=0; i<sizeof(intervals)/sizeof(intervals[0]); ++i)
        if (c->sample_interval_ms == intervals[i]) return true;
    return false;
}
/* cJSON intentionally accepts some non-JSON number spellings (for example 01).
   Check token syntax before parsing so the HTTP boundary accepts actual JSON. */
static bool json_tokens_valid(const char *text, size_t len) {
    size_t i=0;
    while (i<len) {
        unsigned char ch=(unsigned char)text[i];
        if (ch=='"') {
            ++i;
            bool closed=false;
            while (i<len) {
                ch=(unsigned char)text[i++];
                if (ch=='"') { closed=true; break; }
                if (ch<32) return false;
                if (ch=='\\') {
                    if (i==len) return false;
                    /* Reject embedded NUL in decoded keys, not just raw body. */
                    if (len-i>=5 && memcmp(text+i,"u0000",5)==0) return false;
                    ++i;
                }
            }
            if (!closed) return false;
        } else if (ch=='-' || (ch>='0' && ch<='9')) {
            if (ch=='-' && ++i==len) return false;
            if (text[i]=='0') ++i;
            else {
                if (text[i]<'1' || text[i]>'9') return false;
                do { ++i; } while (i<len && text[i]>='0' && text[i]<='9');
            }
            if (i<len && text[i]=='.') {
                ++i; size_t first=i;
                while (i<len && text[i]>='0' && text[i]<='9') ++i;
                if (i==first) return false;
            }
            if (i<len && (text[i]=='e' || text[i]=='E')) {
                ++i;
                if (i<len && (text[i]=='+' || text[i]=='-')) ++i;
                size_t first=i;
                while (i<len && text[i]>='0' && text[i]<='9') ++i;
                if (i==first) return false;
            }
            if (i<len && !strchr(" ,}]:\t\r\n",text[i])) return false;
        } else {
            if (ch<32 && ch!='\t' && ch!='\r' && ch!='\n') return false;
            ++i;
        }
    }
    return true;
}
bool catscan_config_parse(const char *text, size_t length, catscan_config_t *out) {
    if (!text || !out || length == 0 || length > 256 || memchr(text, 0, length) || !json_tokens_valid(text,length)) return false;
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(text, length, &end, false);
    if (!root) return false;
    while (end < text + length && (*end==' ' || *end=='\r' || *end=='\n' || *end=='\t')) ++end;
    bool ok = cJSON_IsObject(root) && end == text + length;
    unsigned seen = 0;
    catscan_config_t c = {0};
    const cJSON *field = NULL;
    cJSON_ArrayForEach(field, root) {
        if (!field->string || !cJSON_IsNumber(field) || !isfinite(field->valuedouble) ||
            floor(field->valuedouble) != field->valuedouble) { ok=false; break; }
        if (strcmp(field->string,"sample_interval_ms")==0 && !(seen & 1) &&
            field->valuedouble >= 100 && field->valuedouble <= 10000) {
            c.sample_interval_ms=(uint32_t)field->valuedouble; seen |= 1;
        } else if (strcmp(field->string,"gain")==0 && !(seen & 2) &&
                   field->valuedouble >= 0 && field->valuedouble <= 5) {
            c.gain=(unsigned)field->valuedouble; seen |= 2;
        } else { ok=false; break; }
    }
    ok = ok && seen == 3 && catscan_config_valid(&c);
    cJSON_Delete(root);
    if (ok) *out=c;
    return ok;
}
cJSON *catscan_config_json(const catscan_config_t *c) {
    cJSON *o=cJSON_CreateObject();
    if (!o) return NULL;
    if (!cJSON_AddNumberToObject(o,"sample_interval_ms",c->sample_interval_ms) ||
        !cJSON_AddNumberToObject(o,"gain",c->gain) ||
        !cJSON_AddNumberToObject(o,"full_scale_v",catscan_full_scale(c->gain)) ||
        !cJSON_AddNumberToObject(o,"data_rate_sps",CATSCAN_DATA_RATE)) { cJSON_Delete(o); return NULL; }
    return o;
}
