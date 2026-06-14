#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "main.h"
#include "session.h"
#include <masterpilot/customdata-stream-tx.h>

typedef struct {
    uint16_t tu;
    uint8_t *acc;
    uint16_t acc_fill;
} tx_user_t;

static void push_cb(void *raw, mp_tx_action_t act, const mp_header_t *hdr,
                    const uint8_t *data, uint16_t size)
{
    tx_user_t *u = (tx_user_t *)raw;

    switch (act) {
    case MP_TX_ACT_DATA:
        if (data && size) {
            memcpy(u->acc + u->acc_fill, data, size);
            u->acc_fill += size;
        }
        if (hdr) {
            uint8_t block[4096];
            memset(block, 0, u->tu);
            mp_header_pack(block, hdr);
            memcpy(block + MP_HEADER_SIZE, u->acc, mp_max_payload((mp_config_t){u->tu}));
            write(STDOUT_FILENO, block, u->tu);
            u->acc_fill = 0;
        }
        break;
    case MP_TX_ACT_FINALIZE:
        {
            uint8_t block[4096];
            memset(block, 0, u->tu);
            mp_header_pack(block, hdr);
            memcpy(block + MP_HEADER_SIZE, u->acc, u->acc_fill);
            write(STDOUT_FILENO, block, u->tu);
            u->acc_fill = 0;
        }
        break;
    }
}

int cmd_tx(int argc, char *argv[])
{
    uint16_t stdin_buf_size;
    const char *session_path = tx_parse_args(argc, argv, &stdin_buf_size);
    if (!session_path) return 1;

    mp_tx_session_t *session = mp_session_tx_load(session_path);
    if (!session) return 1;

    mp_config_t cfg = { .transmission_unit = session->transmission_unit };
    uint16_t max_p = mp_max_payload(cfg);

    uint8_t *acc = (uint8_t *)malloc(max_p);
    if (!acc) return 1;

    tx_user_t user = { .tu = session->transmission_unit, .acc = acc, .acc_fill = 0 };

    mp_tx_stream_t stream;
    mp_tx_stream_init(&stream, cfg, session->coord,
        (mp_tx_sink_t){ .push = push_cb, .user = &user });

    uint8_t *buf = (uint8_t *)malloc(stdin_buf_size);
    if (!buf) { free(acc); return 1; }

    ssize_t n;
    while ((n = read(STDIN_FILENO, buf, stdin_buf_size)) > 0)
        mp_tx_stream_feed(&stream, buf, (uint16_t)n);

    mp_tx_stream_finalize(&stream);

    stream.cursor.package_id++;
    stream.cursor.offset = 0;

    free(buf);
    free(acc);
    session->coord = stream.cursor;
    mp_session_tx_save(session);
    return 0;
}