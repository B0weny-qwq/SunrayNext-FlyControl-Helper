set(common_dir "${CMAKE_CURRENT_LIST_DIR}/../common")

if(NOT IS_DIRECTORY "${common_dir}")
	message(FATAL_ERROR "Expected vendored common directory at ${common_dir}")
endif()

set(COMPONENT_SRCS
	"${common_dir}/proto/esp_hosted_rpc.pb-c.c"
	"app/service/coprocessor_entry.c"
	"upstream/protocomm_pserial.c"
	"upstream/esp_hosted_coprocessor.c"
	"upstream/mempool.c"
	"upstream/stats.c"
	"upstream/mempool_ll.c"
	"upstream/host_power_save.c"
	"upstream/slave_light_sleep.c"
	"upstream/slave_transport_gpio_pin_guard.c"
	"upstream/slave_gpio_expander.c"
	"upstream/slave_ext_coex.c"
	"upstream/slave_control.c"
)

if(CONFIG_ESP_HOSTED_CP_BT)
	list(APPEND COMPONENT_SRCS "legacy/optional/slave_bt.c")
endif()

if(CONFIG_ESP_HOSTED_CP_WIFI)
	list(APPEND COMPONENT_SRCS "upstream/slave_wifi_std.c")
endif()

if(CONFIG_ESP_WIFI_ENTERPRISE_SUPPORT)
	list(APPEND COMPONENT_SRCS "legacy/optional/slave_wifi_enterprise.c")
endif()

if (CONFIG_ESP_HOSTED_NETWORK_SPLIT_ENABLED)
	list(APPEND COMPONENT_SRCS "legacy/optional/slave_network_split.c" "legacy/optional/nw_split_router.c")
endif()

set(COMPONENT_ADD_INCLUDEDIRS
	"."
	"upstream"
	"legacy/transport"
	"legacy/optional"
	"legacy/examples"
	"app/service"
	"control/service"
	"wifi/service"
	"transport/service"
	"config/model"
	"${common_dir}"
	"${common_dir}/log"
	"${common_dir}/proto"
	"${common_dir}/rpc"
	"${common_dir}/transport"
)

if(CONFIG_ESP_HOSTED_CP_BT)
# Select BT UART code based on IDF Target
if(CONFIG_IDF_TARGET_ESP32)
	list(APPEND COMPONENT_SRCS legacy/optional/slave_bt_uart_esp32.c)
elseif(CONFIG_IDF_TARGET_ESP32C3 OR CONFIG_IDF_TARGET_ESP32S3)
	list(APPEND COMPONENT_SRCS legacy/optional/slave_bt_uart_esp32c3_s3.c)
else()
	list(APPEND COMPONENT_SRCS legacy/optional/slave_bt_uart_esp32xx.c)
endif()
endif()

if(CONFIG_ESP_SDIO_HOST_INTERFACE)
	list(APPEND COMPONENT_SRCS upstream/sdio_slave_api.c)
elseif(CONFIG_ESP_SPI_HOST_INTERFACE)
	list(APPEND COMPONENT_SRCS legacy/transport/spi_slave_api.c)
elseif(CONFIG_ESP_SPI_HD_MODE)
	list(APPEND COMPONENT_SRCS legacy/transport/spi_hd_slave_api.c)
else(CONFIG_ESP_UART_HOST_INTERFACE)
	list(APPEND COMPONENT_SRCS legacy/transport/uart_slave_api.c)
endif()

if(CONFIG_EXAMPLE_PEER_DATA_TRANSFER)
	list(APPEND COMPONENT_SRCS legacy/examples/example_peer_data_transfer.c)
endif()

if(CONFIG_ESP_HOSTED_COPROCESSOR_EXAMPLE_LIGHT_SLEEP)
	list(APPEND COMPONENT_SRCS legacy/examples/example_light_sleep.c)
endif()

if(CONFIG_ESP_HOSTED_COPROCESSOR_EXAMPLE_MQTT)
	list(APPEND COMPONENT_SRCS legacy/examples/example_mqtt_client.c)
endif()

if(CONFIG_ESP_HOSTED_COPROCESSOR_EXAMPLE_HTTP_CLIENT)
	list(APPEND COMPONENT_SRCS legacy/examples/example_http_client.c)
endif()

# cli
list(APPEND COMPONENT_ADD_INCLUDEDIRS "${common_dir}/utils")

register_component()

# Add directory of protocomm_priv.h to include paths
idf_component_get_property(protocomm_dir protocomm COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${protocomm_dir}/src/common")

# Add linker options to wrap esp_wifi_init function
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_wifi_init")
