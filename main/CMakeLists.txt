idf_component_register(SRCS "prox.c" "page_weather.c" "page_adhan.c" "page_home.c"
                    INCLUDE_DIRS "."
                    REQUIRES wifi_manager openweather adhan espu8g2 lwip)
