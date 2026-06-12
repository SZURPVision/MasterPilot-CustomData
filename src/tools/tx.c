#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "main.h"
#include "session.h"
#include <masterpilot/customdata-stream-tx.h>

static void stdout_block_push(void *user, const mp_header_t *hdr,
                               const uint8_t *payload, uint16_t payload_size)
{
    uint16_t *tu = (uint16_t *)user;
    uint8_t  block[*tu];
    memset(block, 0, *tu);

    memcpy(block, hdr, sizeof(mp_header_t));
    if (payload_size > 0) memcpy(block + sizeof(mp_header_t), payload, payload_size);

    const uint8_t *p = block;
    uint16_t remaining = *tu;
    while (remaining > 0) {
        ssize_t n = write(STDOUT_FILENO, p, remaining);
        if (n < 0) return;
        p         += (size_t)n;
        remaining -= (uint16_t)n;
    }
}

int cmd_tx(int argc, char *argv[])
{
    uint16_t stdin_buf_size;
    const char *session_path = tx_parse_args(argc, argv, &stdin_buf_size);
    if (!session_path) return 1;

    mp_tx_session_t *session = mp_session_tx_load(session_path);
    if (!session) return 1;

    mp_config_t cfg   = { .transmission_unit = session->transmission_unit };
    uint16_t max_p    = mp_max_payload(cfg);

    mp_stream_sink_t sink = { .push = stdout_block_push, .user = &cfg.transmission_unit };
    mp_tx_stream_context_t ctx = mp_tx_stream_init(cfg, sink, session->coord);

    uint8_t *acc  = (uint8_t *)malloc(max_p);
    uint8_t *snip = (uint8_t *)malloc(stdin_buf_size);
    if (!acc || !snip) { free(acc); free(snip); return 1; }

    ssize_t  nread;
    uint16_t fill = 0;

    while ((nread = read(STDIN_FILENO, snip, stdin_buf_size)) > 0) {
        uint16_t off = 0;
        while (off < (uint16_t)nread) {
            uint16_t room = (uint16_t)(max_p - fill);
            uint16_t take = (uint16_t)((uint16_t)nread - off) < room ? (uint16_t)((uint16_t)nread - off) : room;

            memcpy(acc + fill, snip + off, take);
            fill += take;
            off  += take;

            if (fill == max_p) {
                mp_tx_encode_stream(&ctx, acc, max_p, false);
                fill = 0;
            }
        }
    }

    if (fill > 0) {
        mp_tx_encode_stream(&ctx, acc, fill, true);
    } else {
        mp_tx_encode_stream(&ctx, NULL, 0, true);
    }

    /* Start next package at offset 0, package_id incremented */
    ctx.cursor.package_id++;
    ctx.cursor.offset = 0;

    free(snip);
    free(acc);
    session->coord = ctx.cursor;
    mp_session_tx_save(session);
    return 0;
}