idf_component_register(SRCS "app_main.c" INCLUDE_DIRS "." REQUIRES catscan_adc catscan_config catscan_model catscan_http esp_wifi esp_event esp_netif nvs_flash esp_timer esp_driver_usb_serial_jtag)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
