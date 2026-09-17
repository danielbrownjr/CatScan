#pragma once
#include "adc_math.h"
#include "cJSON.h"
#include <stddef.h>
typedef struct { uint32_t sample_interval_ms; unsigned gain; } catscan_config_t;
catscan_config_t catscan_config_default(void);
bool catscan_config_valid(const catscan_config_t *config);
/* Exact complete object: rejects duplicate, unknown, missing and malformed fields. */
bool catscan_config_parse(const char *json, size_t length, catscan_config_t *out);
cJSON *catscan_config_json(const catscan_config_t *config);
