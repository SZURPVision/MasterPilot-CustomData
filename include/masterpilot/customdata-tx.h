#ifndef __MP_CUSTOMDATA_TX_H_
#define __MP_CUSTOMDATA_TX_H_

#include "customdata-common.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma region TX Pure Operators

/*
 * @brief 发送小分片状态
 */
typedef struct {
    mp_header_meta_t     header /**< 当前分片的header. header跳变即为slice边界 */;
    mp_coordinate_t next /**< 下一个状态的坐标 */;
} mp_tx_slice_t;

/*
 * @brief 推进坐标
 */
MP_PURE
MP_INLINE mp_coordinate_t mp_tx_advance(
    const mp_config_t     config,
    const mp_coordinate_t current
)
{
    const uint16_t max_payload = mp_max_payload(config);
    mp_coordinate_t result = {
        .sender_id  = current.sender_id,
        .package_id = current.package_id,
        .offset     = (uint16_t)(current.offset + max_payload)
    };
	return result;
}

/*
 * @brief 构建 wire header
 * @param is_final 本 slice 是否为该 package 的最后一个 slice
 */
MP_PURE
MP_INLINE mp_header_meta_t mp_tx_make_header(
    const mp_config_t     config,
    const mp_coordinate_t current,
    const uint16_t        payload_size,
    const bool            eop
)
{
    const uint16_t max_payload = mp_max_payload(config);
    mp_header_meta_t result = {
        .package_serial     = current.package_id,
        .slice_serial       = mp_offset_to_slice_idx(current.offset,max_payload),
        .sender_id          = (uint8_t)(current.sender_id & 0x7u),
        .eop                = eop,
        .slice_payload_size = payload_size
    };
	return result;
}

/*
 * @brief 一步完整准备 = mp_tx_make_header + mp_tx_advance
 */
MP_PURE
MP_INLINE mp_tx_slice_t mp_tx_prepare(
    const mp_config_t     config,
    const mp_coordinate_t current,
    const uint16_t        payload_size,
    const bool            is_final
)
{
    mp_tx_slice_t result = {
        .header = mp_tx_make_header(config, current, payload_size, is_final),
        .next   = mp_tx_advance(config, current)
    };
	return result;
}

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif