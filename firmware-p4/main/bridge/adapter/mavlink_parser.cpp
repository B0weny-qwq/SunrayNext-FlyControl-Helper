#include "bridge/adapter/mavlink_parser.h"

#include <string.h>

#include "bridge/model/bridge_config.h"

static uint16_t mavlink_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

bool mavlink_is_frame(const uint8_t *data, size_t len) {
    return data && len >= MAVLINK_MIN_FRAME_LEN && data[0] == MAVLINK_STX_V1;
}

uint8_t mavlink_get_sequence(const uint8_t *data) {
    return mavlink_is_frame(data, MAVLINK_MIN_FRAME_LEN) ? data[2] : 0;
}

uint8_t mavlink_get_msg_id(const uint8_t *data) {
    return mavlink_is_frame(data, MAVLINK_MIN_FRAME_LEN) ? data[5] : 0;
}

uint8_t mavlink_get_payload_len(const uint8_t *data) {
    return mavlink_is_frame(data, MAVLINK_MIN_FRAME_LEN) ? data[1] : 0;
}

bool mavlink_verify_crc(const uint8_t *data, size_t len) {
    if (!mavlink_is_frame(data, len)) {
        return false;
    }

    const uint8_t payload_len = data[1];
    const size_t frame_len = payload_len + MAVLINK_MIN_FRAME_LEN;
    if (len < frame_len) {
        return false;
    }

    uint8_t crc_buf[MAVLINK_MAX_PAYLOAD_LEN + 2];
    crc_buf[0] = data[5];
    memcpy(&crc_buf[1], &data[6], payload_len);

    const uint16_t crc = mavlink_crc16(crc_buf, payload_len + 1);
    const uint16_t frame_crc = (uint16_t)(data[frame_len - 1] | (data[frame_len - 2] << 8));
    return crc == frame_crc;
}
