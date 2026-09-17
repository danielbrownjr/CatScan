#include "ads1115.h"
#include "catscan_http.h"
#include "driver/usb_serial_jtag.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
_Static_assert(configTICK_RATE_HZ == 1000, "CatScan timing requires CONFIG_FREERTOS_HZ=1000");
static catscan_state_t state;
static SemaphoreHandle_t state_lock;
static QueueHandle_t csv_queue;
static TaskHandle_t sampler_task;
typedef struct { char text[256]; } csv_line_t;
static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time()/1000); }
static void snapshot(catscan_state_t *out, uint64_t *now) {
    xSemaphoreTake(state_lock,portMAX_DELAY); *out=state; *now=now_ms(); xSemaphoreGive(state_lock);
}
static bool configure(catscan_config_t c) {
    if (!catscan_config_valid(&c)) return false;
    xSemaphoreTake(state_lock,portMAX_DELAY);
    catscan_state_configure(&state,c);
    xSemaphoreGive(state_lock);
    if (sampler_task) xTaskNotifyGive(sampler_task);
    return true;
}
static void usb_write_all(const char *text) {
    size_t left=strlen(text);
    while (left) {
        int n=usb_serial_jtag_write_bytes(text,left,pdMS_TO_TICKS(20));
        if (n>0) { text+=n; left-=(size_t)n; }
        else vTaskDelay(pdMS_TO_TICKS(10));
    }
}
static void csv_writer(void *arg) {
    (void)arg;
    usb_write_all(catscan_csv_header());
    csv_line_t line;
    for (;;) if (xQueueReceive(csv_queue,&line,portMAX_DELAY)==pdTRUE) usb_write_all(line.text);
}
static catscan_error_t read_error(esp_err_t e) {
    if (e==ESP_OK) return CS_OK;
    if (e==ADS1115_ERR_CONVERSION_TIMEOUT) return CS_CONVERSION_TIMEOUT;
    if (e==ESP_ERR_INVALID_RESPONSE) return CS_CONFIG_MISMATCH;
    return CS_I2C_ERROR;
}
static void sampler(void *arg) {
    (void)arg;
    ads1115_t adc={0}; bool initialized=false;
    for (;;) {
        catscan_state_t scan; uint64_t start;
        snapshot(&scan,&start);
        if (!initialized) initialized=ads1115_init(&adc)==ESP_OK;
        esp_err_t probe=initialized ? ads1115_probe(&adc) : ESP_FAIL;
        scan.present=probe==ESP_OK;
        scan.adc_error=probe==ESP_OK ? CS_OK : (probe==ESP_ERR_NOT_FOUND ? CS_ADC_ABSENT : CS_I2C_ERROR);
        for (unsigned i=0;i<CATSCAN_CHANNELS;++i) {
            scan.channels[i]=(catscan_channel_t){.error=scan.adc_error};
            if (scan.present) scan.channels[i].error=read_error(ads1115_read_channel(&adc,i,scan.config.gain,&scan.channels[i].raw));
            if (scan.channels[i].error!=CS_OK && scan.adc_error==CS_OK) scan.adc_error=scan.channels[i].error;
        }
        scan.sample_ms=now_ms(); scan.sampled=true;
        uint64_t elapsed=scan.sample_ms-start;
        xSemaphoreTake(state_lock,portMAX_DELAY);
        /* Never label an old scan with a newly requested PGA or interval. */
        if (state.config_revision==scan.config_revision) {
            scan.csv_dropped_rows=state.csv_dropped_rows;
            scan.scan_overruns=state.scan_overruns+(elapsed>scan.config.sample_interval_ms ? 1 : 0);
            csv_line_t line;
            if (!catscan_csv_row(&scan,line.text,sizeof(line.text)) || xQueueSend(csv_queue,&line,0)!=pdTRUE) ++scan.csv_dropped_rows;
            state=scan;
        }
        xSemaphoreGive(state_lock);
        uint32_t wait_ms=elapsed<scan.config.sample_interval_ms ? scan.config.sample_interval_ms-(uint32_t)elapsed : 1;
        /* No catch-up bursts. Configuration changes wake the sampler immediately. */
        ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(wait_ms));
    }
}
static void start_wifi(void) {
    const char *password=CONFIG_CATSCAN_AP_PASSWORD;
    if (strlen(password)<8 || strlen(password)>63) {
        ESP_LOGE("catscan","AP password must be 8..63 characters; HTTP disabled"); return;
    }
    esp_err_t err=nvs_flash_init();
    if (err!=ESP_OK) { ESP_LOGE("catscan","NVS init failed: %s; HTTP disabled",esp_err_to_name(err)); return; }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (!esp_netif_create_default_wifi_ap()) { ESP_LOGE("catscan","netif allocation failed"); return; }
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    uint8_t mac[6]; ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP));
    wifi_config_t config={0};
    snprintf((char *)config.ap.ssid,sizeof(config.ap.ssid),"CatScan-%02X%02X%02X",mac[3],mac[4],mac[5]);
    memcpy(config.ap.password,password,strlen(password));
    config.ap.authmode=WIFI_AUTH_WPA2_PSK; config.ap.max_connection=2; config.ap.channel=1;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI("catscan","AP %s; http://192.168.4.1",config.ap.ssid);
    ESP_ERROR_CHECK(catscan_http_start(snapshot,configure));
}
void app_main(void) {
    catscan_state_init(&state);
    state_lock=xSemaphoreCreateMutex(); csv_queue=xQueueCreate(16,sizeof(csv_line_t));
    if (!state_lock || !csv_queue) { ESP_LOGE("catscan","allocation failed"); return; }
    usb_serial_jtag_driver_config_t usb=USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    if (xTaskCreate(csv_writer,"csv",3072,NULL,2,NULL)!=pdPASS ||
        xTaskCreate(sampler,"sampler",4096,NULL,5,&sampler_task)!=pdPASS) {
        ESP_LOGE("catscan","task creation failed"); return;
    }
    start_wifi();
}
