#pragma once
#include "catscan_model.h"
#include "esp_err.h"
typedef void (*catscan_snapshot_fn)(catscan_state_t *out, uint64_t *now);
typedef bool (*catscan_configure_fn)(catscan_config_t config);
esp_err_t catscan_http_start(catscan_snapshot_fn snapshot, catscan_configure_fn configure);
