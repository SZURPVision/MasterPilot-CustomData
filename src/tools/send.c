#include "main.h"
#include "session.h"
#include <masterpilot/customdata-core.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

static uint16_t stdout_consumer(const uint8_t *source_block, const uint16_t block_length, void *user)
{
    (void)user;
    const uint8_t *p = source_block;
    uint16_t remaining = block_length;
    while (remaining > 0) {
        ssize_t n = write(STDOUT_FILENO, p, remaining);
        if (n < 0) return 0;
        p += n;
        remaining -= (uint16_t)n;
    }
    return block_length;
}

int cmd_send(int argc, char *argv[])
{
    uint16_t stdin_buf_size;
    const char *session_path = send_parse_args(argc, argv, &stdin_buf_size);
    if (!session_path) return 1;

    mp_sender_t sender = {};
    if (!mp_session_send_load(&sender, session_path))
        return 1;

    mp_block_writer_t writer;
    uint16_t remaining = MP_BlockWriter_Begin(&writer, &sender);
    uint16_t total = 0;
    uint8_t  *buf = (uint8_t *)malloc(stdin_buf_size);

    while (remaining > 0) {
        uint16_t ask = remaining < stdin_buf_size ? remaining : stdin_buf_size;
        ssize_t n = read(STDIN_FILENO, buf, ask);
        if (n <= 0) break;
        uint16_t w = MP_BlockWriter_Write(&writer, buf, (uint16_t)n);
        total += w;
        remaining -= w;
    }

    free(buf);
    MP_BlockWriter_Commit(&writer, total);
    MP_Send(&sender, stdout_consumer, NULL);

    mp_session_send_save(&sender, session_path);
    mp_session_free(sender.config);
    return 0;
}