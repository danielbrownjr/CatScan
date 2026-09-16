#include "ads1115.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define BUS_TIMEOUT_MS 10
esp_err_t ads1115_init(ads1115_t *adc) {
    if (!adc) return ESP_ERR_INVALID_ARG;
    *adc=(ads1115_t){0};
    i2c_master_bus_config_t cfg={.i2c_port=I2C_NUM_0,.sda_io_num=GPIO_NUM_5,
        .scl_io_num=GPIO_NUM_6,.clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7};
    esp_err_t err=i2c_new_master_bus(&cfg,&adc->bus);
    if (err!=ESP_OK) return err;
    i2c_device_config_t dev={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=0x48,.scl_speed_hz=100000};
    err=i2c_master_bus_add_device(adc->bus,&dev,&adc->device);
    if (err!=ESP_OK) { i2c_del_master_bus(adc->bus); adc->bus=NULL; }
    return err;
}
esp_err_t ads1115_probe(ads1115_t *adc) {
    return i2c_master_probe(adc->bus,0x48,BUS_TIMEOUT_MS);
}
esp_err_t ads1115_read_register(ads1115_t *adc, uint8_t reg, uint16_t *value) {
    if (!adc || !adc->device || !value || reg>3) return ESP_ERR_INVALID_ARG;
    uint8_t bytes[2];
    esp_err_t err=i2c_master_transmit_receive(adc->device,&reg,1,bytes,2,BUS_TIMEOUT_MS);
    if (err==ESP_OK) *value=(uint16_t)(((uint16_t)bytes[0]<<8)|bytes[1]);
    return err;
}
esp_err_t ads1115_write_register(ads1115_t *adc, uint8_t reg, uint16_t value) {
    if (!adc || !adc->device || reg>3) return ESP_ERR_INVALID_ARG;
    uint8_t bytes[]={reg,(uint8_t)(value>>8),(uint8_t)value};
    return i2c_master_transmit(adc->device,bytes,sizeof(bytes),BUS_TIMEOUT_MS);
}
esp_err_t ads1115_read_channel(ads1115_t *adc, unsigned channel, unsigned gain, int16_t *raw) {
    uint16_t cfg=0, status=0;
    if (!raw || !catscan_conversion_config(channel,gain,&cfg)) return ESP_ERR_INVALID_ARG;
    /* Recover after an interrupted conversion or a previous continuous-mode user.
       Enter power-down, then wait for any in-flight conversion to finish. */
    esp_err_t err=ads1115_write_register(adc,1,(uint16_t)(cfg & ~0x8000u));
    if (err!=ESP_OK) return err;
    int64_t deadline=esp_timer_get_time()+20000;
    do {
        err=ads1115_read_register(adc,1,&status);
        if (err!=ESP_OK) return err;
        if (status & 0x8000u) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (esp_timer_get_time()<deadline);
    if (!(status & 0x8000u)) return ADS1115_ERR_CONVERSION_TIMEOUT;
    err=ads1115_write_register(adc,1,cfg);
    if (err!=ESP_OK) return err;
    deadline=esp_timer_get_time()+20000;
    /* Avoid interpreting the pre-start idle state as completion. */
    vTaskDelay(pdMS_TO_TICKS(2));
    do {
        err=ads1115_read_register(adc,1,&status);
        if (err!=ESP_OK) return err;
        if ((status & 0x7fffu)!=(cfg & 0x7fffu)) return ESP_ERR_INVALID_RESPONSE;
        if (status & 0x8000u) {
            uint16_t bits=0;
            err=ads1115_read_register(adc,0,&bits);
            if (err==ESP_OK) *raw=catscan_decode(bits);
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (esp_timer_get_time()<deadline);
    return ADS1115_ERR_CONVERSION_TIMEOUT;
}
