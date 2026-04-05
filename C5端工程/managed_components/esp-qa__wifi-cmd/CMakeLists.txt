
include($ENV{IDF_PATH}/tools/cmake/version.cmake)

# CONFIG_ESP_WIFI_ENABLED added in idf v5.1
if(${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR} VERSION_LESS 5.0)
    set(CONFIG_ESP_WIFI_ENABLED y)
elseif(${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR} VERSION_LESS 5.1)
    set(CONFIG_ESP_WIFI_ENABLED CONFIG_ESP32_WIFI_ENABLED)
endif()

if(CONFIG_ESP_WIFI_ENABLED OR CONFIG_ESP_WIFI_REMOTE_ENABLED OR CONFIG_ESP_HOST_WIFI_ENABLED)
    set(srcs  "src/wifi_common.c"
            "src/wifi_init.c"
            "src/wifi_handler.c"
            "src/sta_cmd.c"
            "src/ap_cmd.c"
            "src/wifi_cmd.c"
            "src/wifi_band_cmd.c"
            "src/wifi_txpower.c"
            "src/wifi_proto_cbw_cmd.c"
            "src/wifi_query_cmd.c")

    # version >= v5.1
    if(${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR} VERSION_GREATER 5.0)
        list(APPEND srcs "src/wifi_stats_cmd.c" "src/wifi_itwt_cmd.c" "src/wifi_he_debug_cmd.c")
    endif()
else()
	# Stub if wifi is not available
	set(srcs "src/unsupported_wifi_cmd.c")
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_netif esp_wifi
                       PRIV_REQUIRES console lwip)

if(CONFIG_ESP_WIFI_ENABLED OR CONFIG_ESP_WIFI_REMOTE_ENABLED OR CONFIG_ESP_HOST_WIFI_ENABLED)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 11.0)
        target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
    endif()
endif()
