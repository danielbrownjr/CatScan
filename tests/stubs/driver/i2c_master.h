#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_TIMEOUT 2
#define ESP_ERR_INVALID_RESPONSE 3
#define ESP_ERR_NOT_FOUND 4
#define I2C_NUM_0 0
#define GPIO_NUM_5 5
#define GPIO_NUM_6 6
#define I2C_CLK_SRC_DEFAULT 0
#define I2C_ADDR_BIT_LEN_7 0
typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
typedef struct { int i2c_port,sda_io_num,scl_io_num,clk_source,glitch_ignore_cnt; } i2c_master_bus_config_t;
typedef struct { int dev_addr_length,device_address,scl_speed_hz; } i2c_device_config_t;
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *,i2c_master_bus_handle_t *);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t,const i2c_device_config_t *,i2c_master_dev_handle_t *);
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t);
esp_err_t i2c_master_probe(i2c_master_bus_handle_t,int,int);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t,const void *,size_t,void *,size_t,int);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t,const void *,size_t,int);
