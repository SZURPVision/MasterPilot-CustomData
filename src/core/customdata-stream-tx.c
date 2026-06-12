#include "customdata-stream-tx.h"
#include "customdata-tx.h"
#include <stddef.h>

mp_tx_stream_context_t mp_tx_stream_init(
    mp_config_t          config,
    mp_stream_sink_t     downstream,
    mp_coordinate_t      start
)
{
    return (mp_tx_stream_context_t){
        .config     = config,
        .cursor     = start,
        .downstream = downstream
    };
}

uint16_t mp_tx_encode_stream(
    mp_tx_stream_context_t *ctx,
    const uint8_t          *data,
    uint16_t                size,
    bool                    is_final_slice
)
{
    const uint16_t max_p = mp_max_payload(ctx->config);
    uint16_t sent = 0;

    while (sent < size) {
        const uint16_t chunk = (size - sent) > max_p ? max_p : (uint16_t)(size - sent);
        const bool     is_last = (sent + chunk >= size) && is_final_slice;

        const mp_tx_slice_t s = mp_tx_prepare(ctx->config, ctx->cursor, chunk, is_last);
        ctx->downstream.push(ctx->downstream.user, &s.header, data + sent, chunk);

        ctx->cursor = s.next;
        sent += chunk;
    }

    /* Emit terminal empty slice when is_final and no partial slice was emitted */
    if (is_final_slice && size == 0) {
        const mp_tx_slice_t s = mp_tx_prepare(ctx->config, ctx->cursor, 0, true);
        ctx->downstream.push(ctx->downstream.user, &s.header, NULL, 0);
        ctx->cursor = s.next;
    }

    return sent;
}
