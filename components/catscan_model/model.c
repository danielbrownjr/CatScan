#include "catscan_model.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
const char *catscan_error_name(catscan_error_t e) {
    static const char *names[]={"ok","not_sampled","adc_absent","i2c_error","conversion_timeout","config_mismatch","stale"};
    return (unsigned)e < sizeof(names)/sizeof(names[0]) ? names[e] : "unknown_error";
}
void catscan_state_configure(catscan_state_t *s, catscan_config_t c) {
    s->config=c; ++s->config_revision; s->sampled=false; s->sample_ms=0;
    for (unsigned i=0;i<CATSCAN_CHANNELS;++i) s->channels[i]=(catscan_channel_t){0,CS_NOT_SAMPLED};
}
void catscan_state_init(catscan_state_t *s) {
    memset(s,0,sizeof(*s)); s->adc_error=CS_NOT_SAMPLED;
    catscan_state_configure(s,catscan_config_default());
}
bool catscan_state_stale(const catscan_state_t *s, uint64_t now) {
    return s->sampled && now > s->sample_ms && now-s->sample_ms > 2ULL*s->config.sample_interval_ms+100;
}
cJSON *catscan_state_json(const catscan_state_t *s, uint64_t now) {
    cJSON *root=cJSON_CreateObject();
    if (!root) return NULL;
    cJSON *device=cJSON_AddObjectToObject(root,"device");
    cJSON *adc=cJSON_AddObjectToObject(root,"adc");
    cJSON *channels=cJSON_AddArrayToObject(root,"channels");
    cJSON *faults=cJSON_AddArrayToObject(root,"faults");
    bool stale=catscan_state_stale(s,now);
#define ADD(x) do { if (!(x)) goto fail; } while (0)
    ADD(device && adc && channels && faults);
    ADD(cJSON_AddStringToObject(device,"product","CatScan"));
    ADD(cJSON_AddStringToObject(device,"firmware_version",CATSCAN_VERSION));
    ADD(cJSON_AddNumberToObject(device,"uptime_ms",(double)now));
    ADD(cJSON_AddBoolToObject(adc,"present",s->present));
    ADD(cJSON_AddNumberToObject(adc,"address",72));
    ADD(cJSON_AddNumberToObject(adc,"gain",s->config.gain));
    ADD(cJSON_AddNumberToObject(adc,"full_scale_v",catscan_full_scale(s->config.gain)));
    ADD(cJSON_AddNumberToObject(adc,"data_rate_sps",CATSCAN_DATA_RATE));
    ADD(cJSON_AddNumberToObject(adc,"sample_interval_ms",s->config.sample_interval_ms));
    ADD(cJSON_AddNumberToObject(adc,"config_revision",s->config_revision));
    if (s->sampled) {
        ADD(cJSON_AddNumberToObject(adc,"last_sample_age_ms",(double)(now>=s->sample_ms ? now-s->sample_ms : 0)));
    } else { ADD(cJSON_AddNullToObject(adc,"last_sample_age_ms")); }
    ADD(cJSON_AddStringToObject(adc,"error",catscan_error_name(s->adc_error)));
    ADD(cJSON_AddNumberToObject(root,"csv_dropped_rows",s->csv_dropped_rows));
    ADD(cJSON_AddNumberToObject(root,"scan_overruns",s->scan_overruns));
    bool seen[CS_STALE+1]={false};
    if (s->adc_error != CS_OK && s->adc_error <= CS_STALE) seen[s->adc_error]=true;
    if (stale) seen[CS_STALE]=true;
    for (unsigned i=0;i<CATSCAN_CHANNELS;++i) {
        cJSON *ch=cJSON_CreateObject(); ADD(ch);
        if (!cJSON_AddItemToArray(channels,ch)) { cJSON_Delete(ch); goto fail; }
        catscan_error_t error=s->channels[i].error;
        if (error==CS_OK && stale) error=CS_STALE;
        bool valid=error==CS_OK && s->sampled;
        ADD(cJSON_AddNumberToObject(ch,"index",i));
        ADD(cJSON_AddBoolToObject(ch,"valid",valid));
        ADD(cJSON_AddStringToObject(ch,"error",catscan_error_name(error)));
        if (valid) {
            ADD(cJSON_AddNumberToObject(ch,"raw",s->channels[i].raw));
            ADD(cJSON_AddNumberToObject(ch,"voltage_v",catscan_voltage(s->channels[i].raw,s->config.gain)));
        } else {
            ADD(cJSON_AddNullToObject(ch,"raw")); ADD(cJSON_AddNullToObject(ch,"voltage_v"));
        }
        if (error != CS_OK && error <= CS_STALE) seen[error]=true;
    }
    for (unsigned i=1;i<=CS_STALE;++i) if (seen[i]) {
        cJSON *f=cJSON_CreateString(catscan_error_name((catscan_error_t)i)); ADD(f);
        if (!cJSON_AddItemToArray(faults,f)) { cJSON_Delete(f); goto fail; }
    }
    return root;
fail:
    cJSON_Delete(root); return NULL;
#undef ADD
}
const char *catscan_csv_header(void) {
    return "timestamp_ms,ch0_raw,ch0_v,ch1_raw,ch1_v,ch2_raw,ch2_v,ch3_raw,ch3_v\n";
}
bool catscan_csv_row(const catscan_state_t *s, char *buf, size_t size) {
    if (!buf || !size || !s->sampled) return false;
    int n=snprintf(buf,size,"%" PRIu64,s->sample_ms);
    if (n<0 || (size_t)n>=size) return false;
    size_t used=(size_t)n;
    for (unsigned i=0;i<CATSCAN_CHANNELS;++i) {
        n=s->channels[i].error==CS_OK ?
            snprintf(buf+used,size-used,",%d,%.6f",s->channels[i].raw,catscan_voltage(s->channels[i].raw,s->config.gain)) :
            snprintf(buf+used,size-used,",,");
        if (n<0 || (size_t)n>=size-used) return false;
        used+=(size_t)n;
    }
    if (size-used<2) return false;
    buf[used++]='\n'; buf[used]=0; return true;
}
