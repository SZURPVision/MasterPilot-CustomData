#include "customdata-stream-rx.h"
#include "customdata-common.h"
#include "customdata-rx.h"

void mp_rx_stream_feed(
    mp_rx_stream_t *s,
    const uint8_t  *frame,
    uint16_t        frame_size
)
{
    if (frame_size < MP_HEADER_SIZE) return;

    const mp_header_packed_t* p_header = (typeof(p_header))frame;
    const mp_header_meta_t hdr = mp_header_unpack(*p_header);

    const uint16_t max_payload    = mp_max_payload(s->config);
    const uint16_t payload_size      = hdr.slice_payload_size;
    const uint32_t offset   = mp_slice_idx_to_offset(hdr.slice_serial, max_payload);
    const uint8_t *payload  = frame + MP_HEADER_SIZE;

    const mp_coordinate_t coord = mp_rx_header_to_coordinate(s->config, hdr);

    const mp_rx_slice_state_t* p_old_state = s->coordinator.state_get(s->coordinator.ctx, coord);


    const mp_rx_action_t action = mp_rx_classify(p_old_state, hdr.slice_serial, offset);

    if (action == MP_RX_ACT_IGNORE) return;

    const mp_rx_slice_inst_t step = mp_calc_slice_step(p_old_state, hdr.slice_serial, payload_size, max_payload, hdr.eop);

    if (action == MP_RX_ACT_STORE) {
        s->coordinator.state_put(s->coordinator.ctx, coord, &step.next_state);
        s->coordinator.payload_put(s->coordinator.ctx, coord, payload, payload_size);
        return;
    }

    /* DELIVER */
    const uint32_t new_wm = mp_rx_advance_watermark(&step.next_state, max_payload);

    uint32_t cur_off = step.next_state.watermark;

    while (cur_off < new_wm) {
        const uint16_t sz = mp_rx_deliver_size(cur_off, new_wm, max_payload);

        mp_coordinate_t dcoord = coord;
        dcoord.offset = cur_off;

        const uint8_t *data = (cur_off == offset) ? payload
                              : s->coordinator.payload_get(s->coordinator.ctx, dcoord);
        if (!data) { cur_off += max_payload; continue; }

        s->on_data(s->user, data, dcoord, sz, hdr.eop);
        cur_off += max_payload;
    }

    if (mp_is_complete(&step.next_state, max_payload)) {
        const mp_rx_slice_state_t zero_state = {0};
        s->coordinator.state_put(s->coordinator.ctx, coord, &zero_state);
    } else {
        mp_rx_slice_state_t to_commit = step.next_state;
        to_commit.watermark = new_wm;
        s->coordinator.state_put(s->coordinator.ctx, coord, &to_commit);
    }
}