#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
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

static void push_cb(
    void *raw,
    mp_tx_action_t act,
    const mp_header_packed_t nullable_header,
    const uint8_t *nullable_data,
    uint16_t size
)
{
    tx_user_t *u = (tx_user_t *)raw;

    switch (act) {
    case MP_TX_ACT_DATA:
        if (nullable_data && size) {
            memcpy(u->acc + u->acc_fill, nullable_data, size);
            u->acc_fill += size;
        }
        // 有header 才发送
        if (nullable_header) {
            uint8_t block[4096];
            mp_header_packed_t* block_header = (mp_header_packed_t*)block;
            *block_header = nullable_header;

            memcpy(block + MP_HEADER_SIZE, u->acc, mp_max_payload((mp_config_t){u->tu}));

            ssize_t n_wrote = write(STDOUT_FILENO, block, u->tu);
            if(n_wrote != u->tu)
            {
                const char errMsg[] = "Failed to write stdout.";
                ssize_t _ = write(STDERR_FILENO, errMsg, sizeof(errMsg));
                exit(1);
            }
            u->acc_fill = 0;
        }
        break;
    case MP_TX_ACT_FINALIZE:
        {
            uint8_t block[4096];
            mp_header_packed_t* block_header = (mp_header_packed_t*)block;
            *block_header = nullable_header;

            memcpy(block + MP_HEADER_SIZE, u->acc, u->acc_fill);

            ssize_t n_wrote = write(STDOUT_FILENO, block, u->tu);
            if(n_wrote != u->tu)
            {
                const char errMsg[] = "Failed to write stdout.";
                ssize_t _ =write(STDERR_FILENO, errMsg, sizeof(errMsg));
                exit(1);
            }
            u->acc_fill = 0;
        }
        break;
    }
}

int cmd_tx(int argc, char *argv[])
{
    uint16_t stdin_buf_size;
    uint16_t sender_id;
    const char *session_path = tx_parse_args(argc, argv, &stdin_buf_size, &sender_id);
    if (!session_path) return 1;

    mp_tx_session_t *session = mp_session_tx_load(session_path);
    if (!session) return 1;

    mp_config_t cfg = { .transmission_unit = session->transmission_unit };
    uint16_t max_p = mp_max_payload(cfg);

    uint8_t *acc = (uint8_t *)malloc(max_p);
    if (!acc) return 1;

    tx_user_t user = { .tu = session->transmission_unit, .acc = acc, .acc_fill = 0 };

    session->coord.sender_id = sender_id;

    mp_tx_stream_t stream = {
        .config = cfg,
        .cursor = session->coord,
        .downstream = {
            .push = push_cb,
            .user = &user
        }
    };

    uint8_t *buf = (uint8_t *)malloc(stdin_buf_size);
    if (!buf) { free(acc); return 1; }

    ssize_t n = -1;
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