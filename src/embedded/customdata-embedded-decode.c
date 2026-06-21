#include <masterpilot/customdata-embedded-decode.h>
#include <masterpilot/customdata-common.h>
#include <masterpilot/customdata-rx.h>
#include <masterpilot/customdata-stream-rx.h>
#include <pb_decode.h>
#include <string.h>
#include <stdlib.h>

/* ── Nanopb slice pull stream ─────────────────────────────────── */

/*
 * @brief nanopb pull stream 状态, 一次解码流程结束后即释放.
 * 不持久化, 不对外暴露.
 */
typedef struct {
    const mp_rx_coordinator_t *coordinator;
    mp_coordinate_t             base_coord;
    uint32_t                    termination;  /* 包总字节数, 来自 mp_rx_slice_state_t.termination */
    uint16_t                    max_payload;
    uint32_t                    consumed;
} mp_pb_slice_pull_t;

static bool mp_pb_slice_pull_callback(pb_istream_t *stream, pb_byte_t *buf, size_t count)
{
    mp_pb_slice_pull_t *st = (mp_pb_slice_pull_t *)stream->state;

    while (count > 0) {
        const uint16_t slice_idx = mp_rx_offset_to_slice_idx(st->consumed, st->max_payload);
        const uint32_t slice_off = mp_slice_idx_to_offset(slice_idx, st->max_payload);

        const uint8_t *src = st->coordinator->payload_get(
            st->coordinator->ctx,
            (mp_coordinate_t){
                .sender_id  = st->base_coord.sender_id,
                .package_id = st->base_coord.package_id,
                .offset     = slice_off
            }
        );

        const uint32_t pos_in_slice = st->consumed - slice_off;
        uint32_t avail = st->termination - st->consumed;
        uint32_t chunk = st->max_payload - pos_in_slice;
        if (chunk > avail) chunk = avail;

        uint32_t take = (uint32_t)count < chunk ? (uint32_t)count : chunk;

        if (buf) memcpy(buf, src + pos_in_slice, take);
        buf            += take;
        st->consumed   += take;
        count          -= take;

        if (st->consumed >= st->termination) break;
    }
    return true;
}

static pb_istream_t mp_pb_slice_istream_init(
    mp_pb_slice_pull_t        *st,
    const mp_rx_coordinator_t *coordinator,
    mp_coordinate_t            base_coord,
    uint32_t                   termination,
    uint16_t                   max_payload
)
{
    *st = (mp_pb_slice_pull_t){
        .coordinator = coordinator,
        .base_coord  = base_coord,
        .termination = termination,
        .max_payload = max_payload,
        .consumed    = 0
    };
    pb_istream_t stream = {
        .callback   = mp_pb_slice_pull_callback,
        .state      = st,
        .bytes_left = termination
    };
    return stream;
}

/* ── 单状态 coordinator ────────────────────────────────────────── */

static const mp_rx_slice_state_t *mp_pb_state_get(void *ctx, mp_coordinate_t coord)
{
    (void)coord;
    mp_pb_decoder_inst_t *inst = (mp_pb_decoder_inst_t *)ctx;
    return &inst->state;
}

static void mp_pb_state_put(void *ctx, mp_coordinate_t coord, const mp_rx_slice_state_t *state)
{
    (void)coord;
    mp_pb_decoder_inst_t *inst = (mp_pb_decoder_inst_t *)ctx;
    inst->state = *state;
}

/* ── 事件回调 ──────────────────────────────────────────────────── */

static void mp_pb_on_event(
    void                       *user,
    mp_rx_stream_event_t        e,
    const mp_coordinate_t      *coord,
    const mp_rx_coordinator_t  *coordinator,
    const mp_rx_slice_state_t  *state,
    uint32_t                    old_watermark,
    const uint8_t              *payload,
    uint16_t                    payload_size
)
{
    mp_pb_decoder_inst_t *inst = (mp_pb_decoder_inst_t *)user;
    (void)old_watermark;

    /* 存当前 payload */
    if (payload_size > 0) {
        coordinator->payload_put(coordinator->ctx, *coord, payload, payload_size);
    }

    /* 包完整 → backpressure 解码 */
    if (e != MP_RX_STREAM_COMPLETE) return;

    /*
     * state->termination 是包总字节数的唯一来源 (mp_rx_calc_slice_step 内部通过
     * mp_rx_calc_termination 计算). 直接读取而非通过包装函数传递, 因为此值即为
     * 所需语义, 无需额外转换.
     */
    mp_pb_slice_pull_t pull_st;
    pb_istream_t istream = mp_pb_slice_istream_init(
        &pull_st,
        coordinator,
        *coord,
        state->termination,
        mp_max_payload(inst->config)
    );

    bool ok = pb_decode(&istream, inst->output_config.fields, inst->output_config.msg);
    if (ok) {
        inst->output_config.on_message(inst->output_config.msg, inst->output_config.user);
        inst->last_decode_ok = true;
    } else {
        inst->last_decode_ok = false;
    }
}

/* ── 公共入口 ──────────────────────────────────────────────────── */

void mp_pb_decode_init(
    mp_pb_decoder_inst_t     *inst,
    mp_rx_payload_setter_t    payload_put,
    mp_rx_payload_getter_t    payload_get
)
{
    inst->stream = (mp_rx_stream_t){
        .config = inst->config,
        .coordinator = {
            .ctx         = inst,
            .state_get   = mp_pb_state_get,
            .state_put   = mp_pb_state_put,
            .payload_put = payload_put,
            .payload_get = payload_get
        },
        .on_event = mp_pb_on_event,
        .user     = inst
    };
    inst->state          = (mp_rx_slice_state_t){0};
    inst->last_decode_ok = true;
}

bool mp_pb_decode_feed(
    mp_pb_decoder_inst_t *inst,
    const uint8_t        *block,
    uint16_t              size
)
{
    mp_rx_stream_feed(&inst->stream, block, size);
    return inst->last_decode_ok;
}
