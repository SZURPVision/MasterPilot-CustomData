#include "customdata-nanopb.h"
#include <stdbool.h>
#include <string.h>
#include <pb_decode.h>

static bool MP_INTERNAL_DecodeStreamCallback(
    pb_istream_t *stream, pb_byte_t *buf, size_t count)
{
    mp_block_reader_t *reader = stream->state;
    uint16_t read = MP_BlockReader_Read(reader, buf, (uint16_t)count);
    return read == count;
}

bool MP_Decode(mp_receiver_t *receiver, const pb_msgdesc_t *fields, void *message)
{
    mp_block_reader_t reader;

    uint16_t total = MP_BlockReader_Begin(&reader, receiver);
    if (total == 0)
        return false;

    pb_istream_t stream = {
        .callback   = MP_INTERNAL_DecodeStreamCallback,
        .state      = &reader,
        .bytes_left = total,
        .errmsg     = NULL
    };

    if (!pb_decode(&stream, fields, message))
        return false;

    MP_BlockReader_Finish(&reader);
    return true;
}