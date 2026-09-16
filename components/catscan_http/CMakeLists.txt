idf_component_register(SRCS "http.c" INCLUDE_DIRS "include" REQUIRES catscan_model esp_http_server EMBED_TXTFILES "index.html")
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
