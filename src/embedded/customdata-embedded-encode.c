#include <masterpilot/customdata-embedded.h>
#include <stdbool.h>
#include <string.h>
#include <pb_encode.h>

static bool MP_INTERNAL_StreamCallback(pb_ostream_t *stream, const pb_byte_t *buf, size_t count)
{
    mp_block_writer_t *writer = stream->state;
    uint16_t written = MP_BlockWriter_Write(writer, buf, (uint16_t)count);
    return written == count;
}

bool MP_Encode(mp_tx_encoder_t *sender, const pb_msgdesc_t *fields, const void *message)
{
    mp_block_writer_t writer;

    uint16_t max_payload = MP_BlockWriter_Begin(&writer, sender);

    pb_ostream_t stream = {
        .callback = MP_INTERNAL_StreamCallback,
        .state = &writer,
        .max_size = max_payload,
        .bytes_written = 0,
        .errmsg = 0
    };

    if (!pb_encode(&stream, fields, message)) {
        MP_BlockWriter_Rollback(&writer);
        return false;
    }

    MP_BlockWriter_Commit(&writer, (uint16_t)stream.bytes_written);
    return true;
}