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
    uint16_t fill;
} tx_user_t;

uint8_t stdout_block[4096];

static void push_cb(
    void *raw,
    mp_tx_action_t act,
    const mp_header_meta_t* nullable_header,
    const uint8_t *nullable_data,
    uint16_t size
)
{
    tx_user_t *u = (tx_user_t *)raw;
    (void) act; //act信息冗余, 足够处理.

    if (nullable_data && size) {
        memcpy(
            stdout_block + u->fill + MP_HEADER_SIZE,
            nullable_data,
            size
        );
        u->fill += size;
    }
    if (nullable_header)
    {
        memcpy(
            stdout_block,
            MP_PACKED_HEADER_TO_ARRAY(mp_header_pack(*nullable_header)),
            MP_HEADER_SIZE
        );

        ssize_t n_wrote = write(STDOUT_FILENO, stdout_block, u->tu);
        if(n_wrote != u->tu)
        {
            const char errMsg[] = "Failed to write stdout.";
            ssize_t _ = write(STDERR_FILENO, errMsg, sizeof(errMsg));
            exit(1);
        }
        u->fill = 0;
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

    tx_user_t user = { .tu = session->transmission_unit, .fill = 0 };

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

    ssize_t n = -1;
    while ((n = read(STDIN_FILENO, buf, stdin_buf_size)) > 0)
        mp_tx_stream_feed(&stream, buf, (uint16_t)n);

    mp_tx_stream_finalize(&stream);

    stream.cursor.package_id++;
    stream.cursor.offset = 0;

    free(buf);
    session->coord = stream.cursor;
    mp_session_tx_save(session);
    return 0;
}