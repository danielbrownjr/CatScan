idf_component_register(SRCS "config.c" INCLUDE_DIRS "include" REQUIRES catscan_adc json)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
