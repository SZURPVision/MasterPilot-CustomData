#include "customdata-stream-rx.h"
#include <string.h>

void mp_rx_stream_init(
    mp_rx_stream_t      *s,
    mp_config_t          config,
    mp_rx_coordinator_t  coordinator,
    mp_rx_data_cb        on_data,
    void                *user
)
{
    s->config      = config;
    s->coordinator = coordinator;
    s->on_data     = on_data;
    s->user        = user;
}

void mp_rx_stream_feed(
    mp_rx_stream_t *s,
    const uint8_t  *frame,
    uint16_t        frame_size
)
{
    if (frame_size < MP_HEADER_SIZE) return;

    mp_header_t hdr;
    mp_header_unpack(&hdr, frame);

    const uint16_t max_p    = mp_max_payload(s->config);
    const uint16_t psz      = hdr.slice_payload_size;
    const uint32_t offset   = mp_slice_idx_to_offset(hdr.slice_serial, max_p);
    const uint8_t *payload  = frame + MP_HEADER_SIZE;
    bool eop = mp_rx_is_slice_eop(hdr);

    mp_coordinate_t coord = mp_rx_header_to_coordinate(s->config, hdr);

    mp_rx_slice_state_t *state = s->coordinator.state_get(s->coordinator.ctx, coord);

    mp_rx_action_t action = mp_rx_classify(state, hdr.slice_serial, offset, state->watermark);

    if (action == MP_RX_ACT_IGNORE) return;

    mp_rx_slice_inst_t inst = mp_calc_slice_step(*state, hdr.slice_serial, psz, max_p, eop);
    *state = inst.next_state;

    if (action == MP_RX_ACT_STORE) {
        s->coordinator.payload_put(s->coordinator.ctx, coord, payload, psz);
        return;
    }

    /* DELIVER */
    uint32_t old_wm = state->watermark;
    uint32_t new_wm = mp_rx_advance_watermark(state->bitmap, old_wm, max_p, state->termination);
    state->watermark = new_wm;

    uint32_t cur_off = old_wm;
    while (cur_off < new_wm) {
        uint16_t sz;
        if (state->termination > 0 && cur_off + max_p > state->termination)
            sz = (uint16_t)(state->termination - cur_off);
        else
            sz = max_p;

        mp_coordinate_t dcoord = { .sender_id = coord.sender_id,
                                   .package_id = coord.package_id,
                                   .offset = cur_off };
        const uint8_t *data = (cur_off == offset) ? payload
                              : s->coordinator.payload_get(s->coordinator.ctx, dcoord);
        if (!data) { cur_off += max_p; continue; }

        s->on_data(s->user, data, dcoord, sz, eop);
        cur_off += max_p;
    }

    if (state->termination > 0 && new_wm >= state->termination) {
        memset(state, 0, sizeof(*state));
    }
}