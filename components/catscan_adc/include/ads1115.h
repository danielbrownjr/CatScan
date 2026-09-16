#pragma once
#include "driver/i2c_master.h"
#include "adc_math.h"
typedef struct { i2c_master_bus_handle_t bus; i2c_master_dev_handle_t device; } ads1115_t;
esp_err_t ads1115_init(ads1115_t *adc);
esp_err_t ads1115_probe(ads1115_t *adc);
esp_err_t ads1115_read_register(ads1115_t *adc, uint8_t reg, uint16_t *value);
esp_err_t ads1115_write_register(ads1115_t *adc, uint8_t reg, uint16_t value);
/* ESP_ERR_INVALID_RESPONSE means config readback did not match. */
esp_err_t ads1115_read_channel(ads1115_t *adc, unsigned channel, unsigned gain, int16_t *raw);
