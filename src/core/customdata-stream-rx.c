#include "customdata-stream-rx.h"
#include "customdata-common.h"
#include "customdata-rx.h"

// 本质计算step, 保存状态, 并透传给下游. 作为纯函数的胶水代码.
void mp_rx_stream_feed(
    mp_rx_stream_t *s,
    const uint8_t  *frame,
    uint16_t        frame_size
)
{
    if (frame_size < MP_HEADER_SIZE) return;

    const mp_header_packed_t* p_header = (typeof(p_header))frame;
    const mp_header_meta_t hdr = mp_header_unpack(*p_header);

    const uint16_t max_payload  = mp_max_payload(s->config);
    const uint16_t payload_size = hdr.slice_payload_size;
    const uint8_t *payload      = frame + MP_HEADER_SIZE;

    const mp_coordinate_t coord = mp_rx_header_to_coordinate(s->config, hdr);

    const mp_rx_slice_state_t* old_state = s->coordinator.state_get(s->coordinator.ctx, coord);

    const uint32_t old_watermark = old_state->watermark;

    const mp_rx_slice_inst_t step = mp_rx_calc_slice_step(
        old_state, hdr.slice_serial, payload_size, max_payload, hdr.eop
    );

    /* DUPLICATE: 纯丢弃, 不通知下游, 不更新状态 */
    if (step.event == MP_RX_STREAM_DUPLICATE) return;

    /* Persist state */
    if (step.event == MP_RX_STREAM_COMPLETE) {
        const mp_rx_slice_state_t zero = {0};
        s->coordinator.state_put(s->coordinator.ctx, coord, &zero);
    } else {
        s->coordinator.state_put(s->coordinator.ctx, coord, &step.next_state);
    }

    s->on_event(
        s->user,
        step.event,
        &coord,
        &s->coordinator,
        &step.next_state,
        old_watermark,
        payload,
        payload_size
    );
}