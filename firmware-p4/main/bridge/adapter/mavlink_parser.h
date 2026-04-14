#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool mavlink_is_frame(const uint8_t *data, size_t len);
uint8_t mavlink_get_sequence(const uint8_t *data);
uint8_t mavlink_get_msg_id(const uint8_t *data);
uint8_t mavlink_get_payload_len(const uint8_t *data);
bool mavlink_verify_crc(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
