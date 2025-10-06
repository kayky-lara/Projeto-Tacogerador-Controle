idf_component_register(SRCS "app_main.c"
                       PRIV_REQUIRES mqtt nvs_flash esp_netif driver esp_adc esp_wifi esp_event log
                       INCLUDE_DIRS ".")