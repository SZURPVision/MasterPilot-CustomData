#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "main.h"
#include "session.h"
#include <masterpilot/customdata-rx.h>

int cmd_rx(int argc, char *argv[])
{
    const char *session_path = require_session_file(argc, argv);
    if (!session_path) return 1;

    uint8_t *assembly = NULL;
    mp_rx_session_t *session = mp_session_rx_load(&assembly, session_path);
    if (!session) return 1;

    const uint16_t tu   = session->transmission_unit;
    const uint16_t max_p = tu - sizeof(mp_header_t);

    /* Read blocks one by one */
    uint8_t *block = (uint8_t *)malloc(tu);
    if (!block) {
        mp_session_rx_save(session, assembly);
        return 1;
    }

    for (;;) {
        /* Read exactly one transmission_unit block */
        uint8_t  *p = block;
        uint16_t  remain = tu;
        while (remain > 0) {
            ssize_t n = read(STDIN_FILENO, p, remain);
            if (n <= 0) goto done;
            p      += (size_t)n;
            remain -= (uint16_t)n;
        }

        const mp_header_t *hdr = (const mp_header_t *)block;
        const uint8_t *payload = block + sizeof(mp_header_t);

        /* ── Rebuild coordinate ── */
        mp_coordinate_t coord = mp_rx_header_to_coordinate(
            (mp_config_t){ .transmission_unit = tu }, *hdr);

        /* ── Check if new package → reset state ── */
        if (coord.package_id != session->current_package_id
            || coord.sender_id != session->current_sender_id)
        {
            memset(&session->state, 0, sizeof(session->state));
            session->current_package_id = coord.package_id;
            session->current_sender_id  = coord.sender_id;
        }

        /* ── Pure step ── */
        bool eop = mp_rx_is_slice_eop((mp_config_t){ .transmission_unit = tu }, *hdr);
        mp_rx_slice_inst_t inst = mp_calc_slice_step(
            session->state, hdr->slice_serial, hdr->slice_payload_size, max_p, eop);

        if (inst.status == MP_STREAM_DUPLICATE) continue;

        session->state = inst.next_state;

        /* ── Write payload to assembly buffer ── */
        memcpy(assembly + coord.offset, payload, hdr->slice_payload_size);

        /* ── Check completion ── */
        if (mp_is_complete(session->state, max_p)) {
            uint32_t total = session->state.termination;
            const uint8_t *p_out = assembly;
            uint32_t remaining = total;
            while (remaining > 0) {
                ssize_t n = write(STDOUT_FILENO, p_out, remaining);
                if (n < 0) goto done;
                p_out     += (uint32_t)n;
                remaining -= (uint32_t)n;
            }

            /* Reset for next package */
            memset(&session->state, 0, sizeof(session->state));
        }
    }

done:
    free(block);
    mp_session_rx_save(session, assembly);
    return 0;
}