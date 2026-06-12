#include "customdata-stream-rx.h"

void mp_rx_stream_feed(
    mp_rx_stream_t *s,
    const uint8_t  *frame,
    uint16_t        frame_size
)
{
    if (s->completed) return;
    if (frame_size < sizeof(mp_header_t)) return;

    const mp_header_t *hdr  = (const mp_header_t *)frame;
    const uint16_t max_p    = mp_max_payload(s->config);
    const uint16_t psz      = hdr->slice_payload_size;
    const uint32_t offset   = mp_slice_idx_to_offset(hdr->slice_serial, max_p);

    /* ── 纯步进 (本地 bitmap) ── */
    bool eop = mp_rx_is_slice_eop(s->config, *hdr);
    mp_rx_slice_inst_t inst = mp_calc_slice_step(
        s->state, hdr->slice_serial, psz, max_p, eop
    );

    if (inst.status == MP_STREAM_DUPLICATE) return;

    s->state = inst.next_state;

    /* ── 决定是否乱序 ── */
    const uint8_t *payload = frame + sizeof(mp_header_t);
    const bool in_order = (offset == s->watermark);

    if (!in_order) {
        /* 乱序: 暂存到 coordinator */
        mp_coordinate_t coord = mp_rx_header_to_coordinate(s->config, *hdr);
        s->coordinator.payload_put(s->coordinator.ctx, coord, payload, psz);
    }

    /* ── 推进 watermark ── */
    uint32_t old_wm = s->watermark;
    s->watermark = mp_rx_advance_watermark(
        s->state.bitmap, old_wm, max_p, s->state.termination
    );

    if (s->watermark <= old_wm) return;

    /* ── 推送 [old_wm, s->watermark) 区间的所有新就绪 slice ── */
    uint32_t cur_off = old_wm;
    while (cur_off < s->watermark) {
        const uint16_t slice_id = mp_offset_to_slice_idx(cur_off, max_p);
        uint16_t slice_size;

        /* 确定本 slice 的 payload 大小 */
        if (s->state.termination > 0 && cur_off + max_p > s->state.termination) {
            slice_size = (uint16_t)(s->state.termination - cur_off);
        } else {
            slice_size = max_p;
        }

        /* 获取 payload 指针：当前帧 (不乱序) 或 coordinator (乱序补缺) */
        const uint8_t *data;
        if (in_order && cur_off == offset) {
            /* 当前 slice: 直接使用帧数据 */
            data = payload;
        } else {
            /* 之前乱序暂存的: 从 coordinator 取出 */
            mp_coordinate_t coord = {
                .sender_id   = hdr->sender_id,
                .package_id  = hdr->package_serial,
                .offset      = cur_off
            };
            data = s->coordinator.payload_get(s->coordinator.ctx, coord);
            if (!data) {
                /* 异常: 应有数据但未找到, 跳过 */
                cur_off += max_p;
                continue;
            }
        }

        bool eop = mp_rx_is_slice_eop(s->config, *hdr)
                || (s->state.termination > 0 && cur_off + slice_size >= s->state.termination);

        s->on_data(s->user, data, cur_off, slice_size, eop);

        cur_off += max_p;

        if (s->state.termination > 0 && cur_off >= s->state.termination) {
            s->completed = true;
            break;
        }
    }
}