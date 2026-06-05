#include "mp-customdata-nanopb.h"
#include "mp-customdata.h"
#include <stdbool.h>
#include <string.h>
#include <pb_encode.h>

typedef struct {
    mp_sender_t *sender;
    uint16_t head_in_slice;
} mp_internal_state_t;

static bool MP_INTERNAL_StreamCallback(pb_ostream_t *stream, const pb_byte_t *buf, size_t count)
{
    mp_internal_state_t *state = stream->state;
    mp_sender_t *sender = state->sender;

    while (count > 0) {
        /* 当前 block 剩余可用空间 */
        size_t available = MP_SENDER_MTU - state->head_in_slice;
        size_t to_write = count < available ? count : available;

        /* 拷贝数据到当前 block */
        memcpy(&sender->buffer[sender->head][state->head_in_slice], buf, to_write);
        buf += to_write;
        count -= to_write;
        state->head_in_slice += to_write;

        /* 当前 block 已写满，切换到下一个 block */
        if (state->head_in_slice >= MP_SENDER_MTU) {
            sender->head = (sender->head + 1) % MP_SENDER_BUFFER_COUNT;
            state->head_in_slice = sizeof(mp_header_t);
        }
    }

    return true;
}

bool MP_Encode(mp_sender_t *sender, const pb_msgdesc_t *fields, const void *message)
{
    /* 保存起始 head 位置，用于失败回滚和帧头回填 */
    uint8_t old_head = sender->head;

    mp_internal_state_t state = {
        .sender = sender,
        .head_in_slice = sizeof(mp_header_t)
    };

    /* 计算可用 payload 容量 */
    uint8_t available_blocks = (MP_SENDER_BUFFER_COUNT + sender->head - sender->tail - 1)
                               % MP_SENDER_BUFFER_COUNT;

    pb_ostream_t stream = {
        .callback = MP_INTERNAL_StreamCallback,
        .state = &state,
        .max_size = (size_t)available_blocks * MP_SENDER_PAYLOAD_MAX_SIZE,
        .bytes_written = 0,
        .errmsg = 0
    };

    if (!pb_encode(&stream, fields, message)) {
        /* 编码失败，回滚 head */
        sender->head = old_head;
        return false;
    }

    /* 提交最后一个可能未写满的 block */
    if (state.head_in_slice > sizeof(mp_header_t))
        sender->head = (sender->head + 1) % MP_SENDER_BUFFER_COUNT;

    /* 统一回填所有 block 的帧头 */
    size_t total_bytes = stream.bytes_written;
    uint8_t slice_serial = 0;

    for (uint8_t blk = old_head; blk != sender->head; blk = (blk + 1) % MP_SENDER_BUFFER_COUNT)
    {
        mp_header_t *header = (mp_header_t *)sender->buffer[blk];
        header->package_serial = sender->serial;
        header->slice_serial = slice_serial++;
        header->slice_payload_size = (total_bytes > MP_SENDER_PAYLOAD_MAX_SIZE)
                                     ? MP_SENDER_PAYLOAD_MAX_SIZE
                                     : (uint16_t)total_bytes;
        total_bytes -= header->slice_payload_size;
    }

    ++sender->serial;
    return true;
}