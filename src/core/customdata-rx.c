#include "customdata-rx.h"

MP_PURE uint16_t mp_offset_to_slice_idx(const uint32_t offset, const uint16_t max_payload)
{
    return (uint16_t)(offset / max_payload);
}

MP_PURE uint32_t mp_calc_termination(
    const uint16_t slice_idx,
    const uint16_t payload_size,
    const uint16_t max_payload)
{
    return mp_slice_idx_to_offset(slice_idx, max_payload) + payload_size;
}

MP_PURE uint32_t mp_termination_to_total_size(const uint32_t termination)
{
    return termination;
}

MP_PURE uint16_t mp_termination_to_slice_count(
    const uint32_t termination,
    const uint16_t max_payload)
{
    return (uint16_t)((termination + max_payload - 1) / max_payload);
}

MP_PURE bool mp_is_complete(
    const mp_rx_slice_state_t state,
    const uint16_t max_payload)
{
    return state.termination > 0
        && state.received_count >= mp_termination_to_slice_count(state.termination, max_payload);
}

MP_PURE mp_rx_action_t mp_rx_classify(
    const mp_rx_slice_state_t *state,
    uint16_t slice_id,
    uint32_t offset,
    uint32_t watermark)
{
    const uint8_t  arr_idx  = (uint8_t)(slice_id >> 5);
    const uint32_t bit_mask = (uint32_t)(1u << (slice_id & 31));

    if (state->bitmap[arr_idx] & bit_mask)
        return MP_RX_ACT_IGNORE;

    if (offset > watermark)
        return MP_RX_ACT_STORE;

    return MP_RX_ACT_DELIVER;
}

MP_PURE bool mp_rx_is_slice_eop(const mp_header_t header)
{
    return header.end_of_package == 1;
}

MP_PURE mp_coordinate_t mp_rx_header_to_coordinate(
    const mp_config_t config,
    const mp_header_t header)
{
    const uint16_t max_p = mp_max_payload(config);
    return (mp_coordinate_t){
        .sender_id  = header.sender_id,
        .package_id = header.package_serial,
        .offset     = mp_slice_idx_to_offset(header.slice_serial, max_p)
    };
}

MP_PURE uint32_t mp_rx_advance_watermark(
    const uint32_t bitmap[8],
    uint32_t       old_watermark,
    uint16_t       max_payload,
    uint32_t       termination)
{
    uint32_t wm = old_watermark;

    while (1) {
        if (termination > 0 && wm >= termination) break;

        const uint16_t  slice_id = mp_offset_to_slice_idx(wm, max_payload);
        const uint8_t   arr_idx  = (uint8_t)(slice_id >> 5);
        const uint32_t  bit_mask = (uint32_t)(1u << (slice_id & 31));

        if (!(bitmap[arr_idx] & bit_mask)) break;

        wm += max_payload;
    }

    if (termination > 0 && wm > termination) {
        wm = termination;
    }

    return wm;
}

MP_PURE mp_rx_slice_inst_t mp_calc_slice_step(
    const mp_rx_slice_state_t state,
    const uint16_t            slice_id,
    const uint16_t            payload_size,
    const uint16_t            max_payload,
    const bool                is_eop
) {
    const uint8_t  arr_idx  = (uint8_t)(slice_id >> 5);
    const uint32_t bit_mask = (uint32_t)(1u << (slice_id & 31));

    /* Duplicate check */
    if (state.bitmap[arr_idx] & bit_mask) {
        return (mp_rx_slice_inst_t){
            .status     = MP_STREAM_DUPLICATE,
            .next_state = state
        };
    }

    /* Construct new pure state */
    uint32_t new_bitmap[8];
    for (int i = 0; i < 8; ++i) {
        new_bitmap[i] = state.bitmap[i];
    }
    new_bitmap[arr_idx] |= bit_mask;

    uint32_t new_term = state.termination;
    if (payload_size < max_payload || is_eop) {
        new_term = mp_calc_termination(slice_id, payload_size, max_payload);
    }

    /* Empty terminal slice does not count toward received_count */
    const uint16_t new_count = (payload_size == 0 && is_eop)
        ? state.received_count
        : state.received_count + 1;

    mp_rx_slice_state_t next_state = {
        .bitmap         = { new_bitmap[0], new_bitmap[1], new_bitmap[2], new_bitmap[3],
                            new_bitmap[4], new_bitmap[5], new_bitmap[6], new_bitmap[7] },
        .received_count = new_count,
        .termination    = new_term
    };

    mp_stream_status_t new_status = mp_is_complete(next_state, max_payload)
        ? MP_STREAM_COMPLETE : MP_STREAM_ACCEPT;

    return (mp_rx_slice_inst_t){
        .status     = new_status,
        .next_state = next_state
    };
}