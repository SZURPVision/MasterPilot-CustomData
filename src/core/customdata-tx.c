#include "customdata-tx.h"

MP_PURE mp_coordinate_t mp_tx_advance(
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

MP_PURE mp_header_t mp_tx_make_header(
    const mp_config_t     config,
    const mp_coordinate_t current,
    const uint16_t        payload_size,
    const bool            is_final
)
{
    const uint16_t max_payload = mp_max_payload(config);
    return (mp_header_t){
        .package_serial     = current.package_id & 0xFF,
        .slice_serial       = (uint16_t)((current.offset / max_payload) & 0xFF),
        .sender_id          = current.sender_id & 0x7,
        .end_of_package     = is_final ? 1 : 0,
        .slice_payload_size = payload_size
    };
}

MP_PURE mp_tx_slice_t mp_tx_prepare(
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