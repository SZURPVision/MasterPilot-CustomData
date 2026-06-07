#include "main.h"
#include "session.h"
#include <customdata-core.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int cmd_recv(int argc, char *argv[])
{
    const char *session_path = require_session_file(argc, argv);
    if (!session_path) return 1;

    mp_receiver_t receiver = {};
    if (!mp_session_recv_load(&receiver, session_path))
        return 1;

    uint16_t mtu = receiver.config.mtu;
    uint8_t  *block_buf = (uint8_t *)malloc(mtu);

    /* Read mtu-byte blocks from stdin, feed to receiver */
    for (;;) {
        uint16_t remaining = mtu;
        uint8_t  *p = block_buf;
        while (remaining > 0) {
            ssize_t n = read(STDIN_FILENO, p, remaining);
            if (n <= 0) goto done;
            p += n;
            remaining -= (uint16_t)n;
        }

        MP_Receive(&receiver, block_buf);

        if (receiver.package_complete) {
            mp_block_reader_t reader;
            uint16_t total = MP_BlockReader_Begin(&reader, &receiver);
            if (total > 0) {
                uint8_t *out = (uint8_t *)malloc(total);
                uint16_t rd = MP_BlockReader_Read(&reader, out, total);
                const uint8_t *wp = out;
                uint16_t wr = rd;
                while (wr > 0) {
                    ssize_t n = write(STDOUT_FILENO, wp, wr);
                    if (n < 0) break;
                    wp += n;
                    wr -= (uint16_t)n;
                }
                free(out);
            }
            MP_BlockReader_Finish(&reader);
        }
    }

done:
    free(block_buf);
    mp_session_recv_save(&receiver, session_path);
    mp_session_free(receiver.config);
    return 0;
}