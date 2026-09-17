#include "ads1115.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int64_t clock_us;
static uint16_t config, raw;
static int64_t ready_at;
static int fail_write, fail_read;
static bool mismatch, stuck, fail_bus, fail_device;
static unsigned deleted, conversions;
int64_t esp_timer_get_time(void) { return clock_us; }
void vTaskDelay(unsigned ticks) { clock_us+=(int64_t)ticks*1000; }
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *c,i2c_master_bus_handle_t *h) {
    assert(c->sda_io_num==5 && c->scl_io_num==6);
    if(fail_bus) return ESP_FAIL;
    *h=(void *)1; return ESP_OK;
}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t h,const i2c_device_config_t *c,i2c_master_dev_handle_t *d) {
    assert(h && c->device_address==0x48 && c->scl_speed_hz==100000);
    if(fail_device) return ESP_FAIL;
    *d=(void *)2; return ESP_OK;
}
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t h) { assert(h); deleted++; return ESP_OK; }
esp_err_t i2c_master_probe(i2c_master_bus_handle_t h,int addr,int timeout) {
    assert(h && addr==0x48 && timeout==10); return ESP_ERR_NOT_FOUND;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d,const void *data,size_t len,int timeout) {
    assert(d && len==3 && timeout==10); if(fail_write) return fail_write;
    const uint8_t *p=data; assert(p[0]==1);
    config=(uint16_t)(((uint16_t)p[1]<<8)|p[2]);
    if(config&0x8000) { ready_at=clock_us+8000; conversions++; }
    return ESP_OK;
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d,const void *tx,size_t nt,void *rx,size_t nr,int timeout) {
    assert(d && nt==1 && nr==2 && timeout==10); if(fail_read) return fail_read;
    uint8_t reg=*(const uint8_t *)tx; uint16_t value=raw;
    if(reg==1) { value=config & 0x7fff; if(!stuck && clock_us>=ready_at)value|=0x8000; if(mismatch)value^=0x0200; }
    else { assert(reg==0 && clock_us>=ready_at); }
    uint8_t *p=rx; p[0]=(uint8_t)(value>>8); p[1]=(uint8_t)value; return ESP_OK;
}
int main(void) {
    ads1115_t adc;
    assert(ads1115_init(NULL)==ESP_ERR_INVALID_ARG);
    fail_bus=true; assert(ads1115_init(&adc)==ESP_FAIL); fail_bus=false;
    fail_device=true; assert(ads1115_init(&adc)==ESP_FAIL && !adc.bus && deleted==1); fail_device=false;
    assert(ads1115_init(&adc)==ESP_OK); assert(ads1115_probe(&adc)==ESP_ERR_NOT_FOUND);
    raw=0xffff;
    for(unsigned ch=0;ch<4;ch++) { int16_t code=0; assert(ads1115_read_channel(&adc,ch,1,&code)==ESP_OK); assert(code==-1); assert(((config>>12)&7)==ch+4); }
    assert(conversions==4);
    int16_t result=123;
    fail_write=ESP_ERR_TIMEOUT;
    assert(ads1115_read_channel(&adc,0,1,&result)==ESP_ERR_TIMEOUT && result==123); fail_write=0;
    fail_read=ESP_FAIL; assert(ads1115_read_channel(&adc,0,1,&result)==ESP_FAIL); fail_read=0;
    mismatch=true; assert(ads1115_read_channel(&adc,0,1,&result)==ESP_ERR_INVALID_RESPONSE); mismatch=false;
    stuck=true; int64_t before=clock_us;
    assert(ads1115_read_channel(&adc,0,1,&result)==ADS1115_ERR_CONVERSION_TIMEOUT);
    assert(clock_us-before>=20000 && clock_us-before<=21000); stuck=false;
    raw=0x8000; assert(ads1115_read_channel(&adc,0,1,&result)==ESP_OK && result==-32768);
    assert(ads1115_read_channel(&adc,4,1,&result)==ESP_ERR_INVALID_ARG);
    puts("PASS: ADC transport, byte order, channel sequencing, readback, bounded timeout, failure recovery");
}
