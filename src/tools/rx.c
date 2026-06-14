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

static mp_rx_slice_state_t *state_get(void *ctx, mp_coordinate_t coord)
{
    rx_ctx_t *c = (rx_ctx_t *)ctx;
    mp_rx_session_t *s = c->session;

    if (coord.package_id != s->current_package_id
        || coord.sender_id != s->current_sender_id)
    {
        s->current_package_id = coord.package_id;
        s->current_sender_id  = coord.sender_id;
        memset(&s->state, 0, sizeof(s->state));
    }
    return &s->state;
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

static void on_data(void *user, const uint8_t *data, mp_coordinate_t coord, uint16_t size, bool eop)
{
    (void)coord;
    (void)eop;
    const uint8_t *p = data;
    uint32_t rem = size;
    while (rem > 0) {
        ssize_t n = write(STDOUT_FILENO, p, rem);
        if (n < 0) return;
        p   += (uint32_t)n;
        rem -= (uint32_t)n;
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

    mp_rx_coordinator_t coord = {
        .ctx         = &rx_ctx,
        .state_get   = state_get,
        .payload_put = payload_put,
        .payload_get = payload_get
    };

    mp_rx_stream_t stream;
    mp_rx_stream_init(&stream,
        (mp_config_t){ .transmission_unit = tu },
        coord, on_data, NULL);

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