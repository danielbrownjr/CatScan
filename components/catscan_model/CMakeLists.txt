idf_component_register(SRCS "model.c" INCLUDE_DIRS "include" REQUIRES catscan_config json)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
