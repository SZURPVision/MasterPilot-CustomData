#include "customdata-stream-tx.h"
#include <stddef.h>

void mp_tx_stream_init(
    mp_tx_stream_t   *s,
    mp_config_t       config,
    mp_coordinate_t   start,
    mp_tx_sink_t      downstream
)
{
    *s = (mp_tx_stream_t){
        .config     = config,
        .cursor     = start,
        .fill       = 0,
        .downstream = downstream
    };
}

void mp_tx_stream_feed(
    mp_tx_stream_t *s,
    const uint8_t  *data,
    uint16_t        size
)
{
    const uint16_t max_p = mp_max_payload(s->config);
    uint16_t off = 0;

    while (off < size) {
        uint16_t take = (uint16_t)(size - off);
        uint16_t room = (uint16_t)(max_p - s->fill);
        if (take > room) take = room;

        s->downstream.push(s->downstream.user, MP_TX_ACT_DATA, NULL, data + off, take);
        s->fill += take;
        off += take;

        if (s->fill == max_p) {
            mp_tx_slice_t slice = mp_tx_prepare(s->config, s->cursor, max_p, false);
            s->downstream.push(s->downstream.user, MP_TX_ACT_DATA, &slice.header, NULL, 0);
            s->cursor = slice.next;
            s->fill = 0;
        }
    }
}

void mp_tx_stream_finalize(mp_tx_stream_t *s)
{
    mp_tx_slice_t slice = mp_tx_prepare(s->config, s->cursor, s->fill, true);
    s->downstream.push(s->downstream.user, MP_TX_ACT_FINALIZE, &slice.header, NULL, 0);
    s->cursor = slice.next;
    s->fill = 0;
}
