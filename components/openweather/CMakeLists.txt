idf_component_register(SRCS "openweather.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_client json esp-tls)
