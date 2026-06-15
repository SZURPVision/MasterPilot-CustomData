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
 * @brief 推进坐标 — 步进一个 slice slot (offset += max_payload)
 * sender_id / package_id 不变，package 边界处理由 coordinator 负责
 */
MP_PURE
inline mp_coordinate_t mp_tx_advance(
    const mp_config_t     config,
    const mp_coordinate_t current
)
{
    const uint16_t max_payload = mp_max_payload(config);
    return (mp_coordinate_t){
        .sender_id  = current.sender_id,
        .package_id = current.package_id,
        .offset     = current.offset + max_payload
    };
}

/*
 * @brief 构建 wire header
 * @param is_final 本 slice 是否为该 package 的最后一个 slice
 */
MP_PURE
inline mp_header_meta_t mp_tx_make_header(
    const mp_config_t     config,
    const mp_coordinate_t current,
    const uint16_t        payload_size,
    const bool            eop
)
{
    const uint16_t max_payload = mp_max_payload(config);
    return (mp_header_meta_t){
        .package_serial     = current.package_id & 0xFF,
        .slice_serial       = (uint16_t)((current.offset / max_payload) & 0xFF),
        .sender_id          = current.sender_id & 0x7,
        .eop                = eop ? 1 : 0,
        .slice_payload_size = payload_size
    };
}

/*
 * @brief 一步完整准备 = mp_tx_make_header + mp_tx_advance
 */
MP_PURE
inline mp_tx_slice_t mp_tx_prepare(
    const mp_config_t     config,
    const mp_coordinate_t current,
    const uint16_t        payload_size,
    const bool            is_final
)
{
    return (mp_tx_slice_t){
        .header = mp_tx_make_header(config, current, payload_size, is_final),
        .next   = mp_tx_advance(config, current)
    };
}

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif