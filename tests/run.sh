#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${IDF_PATH:?Set IDF_PATH to an ESP-IDF checkout (cJSON is used for host tests)}"
mkdir -p tests/build
cc -std=c11 -Wall -Wextra -Werror -Wpedantic -g -fno-omit-frame-pointer \
  -fsanitize=address,undefined -fno-pie -no-pie \
  -Icomponents/catscan_adc/include -Icomponents/catscan_config/include \
  -Icomponents/catscan_model/include -I"$IDF_PATH/components/json/cJSON" \
  components/catscan_adc/adc_math.c components/catscan_config/config.c \
  components/catscan_model/model.c "$IDF_PATH/components/json/cJSON/cJSON.c" \
  tests/test_core.c -lm -o tests/build/test_core
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1}" tests/build/test_core

cc -std=c11 -Wall -Wextra -Werror -Wpedantic -g -fno-omit-frame-pointer \
  -fsanitize=address,undefined -fno-pie -no-pie \
  -Itests/stubs -Icomponents/catscan_adc/include \
  components/catscan_adc/adc_math.c components/catscan_adc/ads1115.c \
  tests/test_driver.c -lm -o tests/build/test_driver
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1}" tests/build/test_driver
