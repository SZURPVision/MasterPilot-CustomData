#include "customdata-common.h"

MP_PURE uint16_t mp_max_payload(const mp_config_t config)
{
    return config.transmission_unit - sizeof(mp_header_t);
}

MP_PURE uint32_t mp_slice_idx_to_offset(const uint16_t slice_idx, const uint16_t max_payload)
{
    return (uint32_t)slice_idx * max_payload;
}

MP_PURE bool mp_header_equals(const mp_header_t a, const mp_header_t b)
{
    return (a.package_serial == b.package_serial)
        && (a.slice_serial == b.slice_serial)
        && (a.sender_id == b.sender_id)
        && (a.end_of_package == b.end_of_package)
        && (a.slice_payload_size == b.slice_payload_size);
}