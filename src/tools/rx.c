#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "session.h"
#include <masterpilot/customdata-stream-rx.h>

typedef struct {
    mp_rx_session_t *session;
    uint8_t         *assembly;
} rx_ctx_t;

static const mp_rx_slice_state_t *state_get(void *ctx, mp_coordinate_t coord)
{
    rx_ctx_t *c = (rx_ctx_t *)ctx;
    (void)coord;
    return &c->session->state;
}

static void state_put(void *ctx, mp_coordinate_t coord, const mp_rx_slice_state_t* state)
{
    rx_ctx_t *c = (rx_ctx_t *)ctx;
    (void)coord;
    c->session->state = *state;
}

static void payload_put(void *ctx, mp_coordinate_t coord, const uint8_t *data, uint16_t size)
{
    rx_ctx_t *c = (rx_ctx_t *)ctx;
    memcpy(c->assembly + coord.offset, data, size);
}

static const uint8_t *payload_get(void *ctx, mp_coordinate_t coord)
{
    rx_ctx_t *c = (rx_ctx_t *)ctx;
    return c->assembly + coord.offset;
}

static void on_event(
    void                       *user,
    mp_rx_stream_event_t        event,
    const mp_coordinate_t      *coord,
    const mp_rx_coordinator_t  *coordinator,
    const mp_rx_slice_state_t  *state,
    uint32_t                    old_watermark,
    const uint8_t              *payload,
    uint16_t                    payload_size
)
{
    rx_ctx_t *c = (rx_ctx_t *)user;

    /* 1. 存当前 payload — 全部非重复 slice 均存入 */
    if (payload_size > 0) {
        coordinator->payload_put(coordinator->ctx, *coord, payload, payload_size);
    }

    /*
     * 2. 冲刷连续区间 [old_watermark, state->watermark)
     *   OUT_OF_ORDER: old == new → 空区间, 仅存不冲刷
     *   IN_ORDER:     old <  new → 冲刷新连续区域
     *   COMPLETE:     old <  new → 冲刷完整包
     * event 枚举在此实现中由 watermark 差值隐式承载, 不再显式分支.
     */
    if (old_watermark < state->watermark) {
        const uint16_t max_p = mp_max_payload(
            (mp_config_t){ .transmission_unit = c->session->transmission_unit }
        );

        uint32_t cur_off = old_watermark;
        while (cur_off < state->watermark) {
            const uint16_t sz = mp_rx_deliver_size(cur_off, state->watermark, max_p);
            const uint8_t *data = coordinator->payload_get(coordinator->ctx,
                (mp_coordinate_t){
                    .sender_id  = coord->sender_id,
                    .package_id = coord->package_id,
                    .offset     = cur_off
                }
            );

            const uint8_t *p = data;
            uint32_t rem = sz;
            while (rem > 0) {
                ssize_t n = write(STDOUT_FILENO, p, rem);
                if (n < 0) return;
                p   += (uint32_t)n;
                rem -= (uint32_t)n;
            }
            cur_off += max_p;
        }
    }
}

int cmd_rx(int argc, char *argv[])
{
    const char *session_path = require_session_file(argc, argv);
    if (!session_path) return 1;

    uint8_t *assembly = NULL;
    mp_rx_session_t *session = mp_session_rx_load(&assembly, session_path);
    if (!session) return 1;

    const uint16_t tu = session->transmission_unit;

    rx_ctx_t rx_ctx = { .session = session, .assembly = assembly };

    mp_rx_coordinator_t coordinator = {
        .ctx         = &rx_ctx,
        .state_get   = state_get,
        .state_put   = state_put,
        .payload_put = payload_put,
        .payload_get = payload_get
    };

    mp_rx_stream_t stream = {
        .config = {
            .transmission_unit = tu
        },
        .coordinator = coordinator,
        .on_event = on_event,
        .user = &rx_ctx
    };

    uint8_t *block = (uint8_t *)malloc(tu);
    if (!block) { mp_session_rx_save(session, assembly); return 1; }

    for (;;) {
        uint8_t  *p = block;
        uint16_t  remain = tu;
        while (remain > 0) {
            ssize_t n = read(STDIN_FILENO, p, remain);
            if (n <= 0) goto done;
            p      += (size_t)n;
            remain -= (uint16_t)n;
        }

        mp_rx_stream_feed(&stream, block, tu);
    }

done:
    free(block);
    mp_session_rx_save(session, assembly);
    return 0;
}