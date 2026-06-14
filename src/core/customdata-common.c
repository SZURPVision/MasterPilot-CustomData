#include "customdata-common.h"

MP_PURE uint16_t mp_max_payload(const mp_config_t config)
{
    return config.transmission_unit - MP_HEADER_SIZE;
}

MP_PURE uint32_t mp_slice_idx_to_offset(const uint16_t slice_idx, const uint16_t max_payload)
{
    return (uint32_t)slice_idx * max_payload;
}