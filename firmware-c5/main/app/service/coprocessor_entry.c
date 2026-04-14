#include "app/service/coprocessor_entry.h"

#include "config/model/product_feature_flags.h"

#if C5_PRODUCT_MAINLINE_SDIO && !CONFIG_ESP_SDIO_HOST_INTERFACE
#error "The product mainline requires CONFIG_ESP_SDIO_HOST_INTERFACE"
#endif

esp_err_t c5_product_profile_check(void) {
    return ESP_OK;
}
