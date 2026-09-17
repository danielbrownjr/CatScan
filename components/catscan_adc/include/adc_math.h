#pragma once
#include <stdbool.h>
#include <stdint.h>
#define CATSCAN_CHANNELS 4
#define CATSCAN_DATA_RATE 128
/* Conservative healthy scan budget, including polling and bus overhead. */
#define CATSCAN_SCAN_BUDGET_MS 80
bool catscan_gain_valid(unsigned gain);
double catscan_full_scale(unsigned gain);
double catscan_voltage(int16_t raw, unsigned gain);
int16_t catscan_decode(uint16_t bits);
bool catscan_conversion_config(unsigned channel, unsigned gain, uint16_t *reg);
