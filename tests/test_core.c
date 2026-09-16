#include "catscan_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void math_tests(void) {
    const double fs[]={6.144,4.096,2.048,1.024,.512,.256};
    for(unsigned g=0;g<6;g++) {
        assert(fabs(catscan_full_scale(g)-fs[g])<1e-12);
        assert(catscan_voltage(0,g)==0);
        assert(fabs(catscan_voltage(-32768,g)+fs[g])<1e-12);
        assert(fabs(catscan_voltage(16384,g)-fs[g]/2)<1e-12);
        assert(fabs(catscan_voltage(32767,g)-fs[g]*32767/32768)<1e-12);
        assert(catscan_voltage(-1,g)<0);
        for(unsigned ch=0;ch<4;ch++) {
            uint16_t word=0;
            assert(catscan_conversion_config(ch,g,&word));
            assert(((word>>12)&7)==ch+4);
            assert(((word>>9)&7)==g);
            assert((word&0x81ff)==0x8183);
        }
    }
    assert(catscan_decode(0xffff)==-1); assert(catscan_decode(0x8000)==-32768);
    assert(catscan_decode(0x7fff)==32767); assert(catscan_decode(0)==0);
    assert(isnan(catscan_voltage(1,6)));
    uint16_t word;
    assert(!catscan_conversion_config(4,1,&word));
    assert(!catscan_conversion_config(0,6,&word));
    assert(!catscan_conversion_config(0,1,NULL));
}
static void config_tests(void) {
    const unsigned intervals[]={100,250,500,1000,2000,5000,10000};
    for(unsigned i=0;i<7;i++) for(unsigned g=0;g<6;g++) {
        catscan_config_t c={intervals[i],g}; assert(catscan_config_valid(&c));
        char text[100]; snprintf(text,sizeof(text),"{\"sample_interval_ms\":%u,\"gain\":%u}",intervals[i],g);
        catscan_config_t parsed={0}; assert(catscan_config_parse(text,strlen(text),&parsed));
        assert(parsed.sample_interval_ms==c.sample_interval_ms && parsed.gain==c.gain);
    }
    for(unsigned i=0;i<=11000;i++) {
        bool expected=false; for(unsigned j=0;j<7;j++) if(i==intervals[j]) expected=true;
        catscan_config_t c={i,1}; assert(catscan_config_valid(&c)==expected);
    }
    const char *bad[]={"", "{", "null", "[]", "{}", "true", "123", "{\"gain\":1}",
      "{\"gain\":1,\"sample_interval_ms\":99}", "{\"gain\":6,\"sample_interval_ms\":100}",
      "{\"gain\":-1,\"sample_interval_ms\":100}", "{\"gain\":1.5,\"sample_interval_ms\":100}",
      "{\"gain\":\"1\",\"sample_interval_ms\":100}", "{\"gain\":true,\"sample_interval_ms\":100}",
      "{\"gain\":1,\"sample_interval_ms\":100,\"x\":0}",
      "{\"gain\":1,\"gain\":2,\"sample_interval_ms\":100}",
      "{\"gain\":1,\"sample_interval_ms\":100,\"sample_interval_ms\":250}",
      "{\"gain\":1,\"sample_interval_ms\":100}{}",
      "{\"gain\":01,\"sample_interval_ms\":100}",
      "{\"gain\":1.,\"sample_interval_ms\":100}",
      "{\"gain\":1e999,\"sample_interval_ms\":100}",
      "{\"gain\":1,\"sample_interval_ms\":100.5}",
      "{\"gain\":1,\"sample_interval_ms\":4294967296}"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        catscan_config_t c=catscan_config_default();
        assert(!catscan_config_parse(bad[i],strlen(bad[i]),&c));
        assert(c.sample_interval_ms==1000 && c.gain==1);
    }
    const char valid[]=" {\"gain\":1,\"sample_interval_ms\":100} \n";
    catscan_config_t c; assert(catscan_config_parse(valid,strlen(valid),&c));
    assert(!catscan_config_parse(valid,sizeof(valid),&c)); /* embedded NUL forbidden */
    cJSON *o=catscan_config_json(&c); assert(o);
    assert(cJSON_GetObjectItemCaseSensitive(o,"full_scale_v")->valuedouble==4.096); cJSON_Delete(o);
}
static void state_tests(void) {
    catscan_state_t s; catscan_state_init(&s);
    cJSON *o=catscan_state_json(&s,0); assert(o);
    cJSON *adc=cJSON_GetObjectItemCaseSensitive(o,"adc");
    assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(adc,"last_sample_age_ms")));
    cJSON *ch=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(o,"channels"),0);
    assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(ch,"raw"))); cJSON_Delete(o);
    s.sampled=true; s.sample_ms=123; s.present=true; s.adc_error=CS_OK;
    for(unsigned i=0;i<4;i++) s.channels[i]=(catscan_channel_t){16384,CS_OK};
    s.channels[1].raw=-1;
    o=catscan_state_json(&s,124); assert(o);
    char *json=cJSON_PrintUnformatted(o); assert(json);
    cJSON *roundtrip=cJSON_Parse(json); assert(roundtrip); cJSON_Delete(roundtrip); cJSON_free(json);
    ch=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(o,"channels"),0);
    assert(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(ch,"valid")));
    assert(cJSON_GetObjectItemCaseSensitive(ch,"voltage_v")->valuedouble==2.048); cJSON_Delete(o);
    assert(!catscan_state_stale(&s,2223)); assert(catscan_state_stale(&s,2224));
    o=catscan_state_json(&s,2224); ch=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(o,"channels"),0);
    assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(ch,"voltage_v"))); cJSON_Delete(o);
    char csv[256]; assert(catscan_csv_row(&s,csv,sizeof(csv)));
    assert(strstr(csv,"123,16384,2.048000,-1,-0.000125"));
    assert(!catscan_csv_row(&s,csv,5));
    s.present=false; s.adc_error=CS_ADC_ABSENT;
    for(unsigned i=0;i<4;i++) s.channels[i].error=CS_ADC_ABSENT;
    assert(catscan_csv_row(&s,csv,sizeof(csv))); assert(strcmp(csv,"123,,,,,,,,\n")==0);
    o=catscan_state_json(&s,124); assert(o);
    assert(strcmp(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(o,"faults"),0)->valuestring,"adc_absent")==0);
    cJSON_Delete(o);
    s.present=true; s.adc_error=CS_I2C_ERROR; s.channels[0].error=CS_I2C_ERROR;
    o=catscan_state_json(&s,124); ch=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(o,"channels"),0);
    assert(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(ch,"valid"))); cJSON_Delete(o);
    uint32_t rev=s.config_revision;
    catscan_state_configure(&s,(catscan_config_t){100,5});
    assert(s.config_revision==rev+1 && !s.sampled && s.channels[0].error==CS_NOT_SAMPLED);
}
int main(void) { math_tests(); config_tests(); state_tests(); puts("PASS: conversion, register encoding, strict config, JSON, faults, stale state, CSV"); }
