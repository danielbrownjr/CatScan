idf_component_register(SRCS "adc_math.c" "ads1115.c" INCLUDE_DIRS "include" REQUIRES esp_driver_i2c esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
