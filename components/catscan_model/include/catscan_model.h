#pragma once
#include "catscan_config.h"
#define CATSCAN_VERSION "0.1.0-dev"
typedef enum { CS_OK, CS_NOT_SAMPLED, CS_ADC_ABSENT, CS_I2C_ERROR, CS_CONVERSION_TIMEOUT,
               CS_CONFIG_MISMATCH, CS_STALE } catscan_error_t;
typedef struct { int16_t raw; catscan_error_t error; } catscan_channel_t;
typedef struct {
    catscan_config_t config;
    uint32_t config_revision;
    bool present;
    bool sampled;
    uint64_t sample_ms;
    catscan_channel_t channels[CATSCAN_CHANNELS];
    uint32_t csv_dropped_rows;
    uint32_t scan_overruns;
    catscan_error_t adc_error;
} catscan_state_t;
const char *catscan_error_name(catscan_error_t e);
void catscan_state_init(catscan_state_t *s);
void catscan_state_configure(catscan_state_t *s, catscan_config_t c);
bool catscan_state_stale(const catscan_state_t *s, uint64_t now);
cJSON *catscan_state_json(const catscan_state_t *s, uint64_t now);
const char *catscan_csv_header(void);
bool catscan_csv_row(const catscan_state_t *s, char *buf, size_t size);
