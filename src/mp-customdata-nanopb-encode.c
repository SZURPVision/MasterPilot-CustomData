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
    uint16_t mtu = sender->config.mtu;

    while (count > 0) {
        // 当前 block 剩余可用空间
        size_t available = mtu - state->head_in_slice;
        size_t to_write = count < available ? count : available;

        // 拷贝数据到当前 block
        uint8_t *block = sender->config.buffer
                         + (size_t)sender->head * mtu;
        memcpy(&block[state->head_in_slice], buf, to_write);
        buf += to_write;
        count -= to_write;
        state->head_in_slice += to_write;

        // 当前 block 已写满，切换到下一个 block
        if (state->head_in_slice >= mtu) {
            sender->head = (sender->head + 1) % sender->config.buffer_count;
            state->head_in_slice = sizeof(mp_header_t);
        }
    }

    return true;
}

bool MP_Encode(mp_sender_t *sender, const pb_msgdesc_t *fields, const void *message)
{
    // 保存起始 head 位置，用于失败回滚和帧头回填
    uint8_t old_head = sender->head;

    mp_internal_state_t state = {
        .sender = sender,
        .head_in_slice = sizeof(mp_header_t)
    };

    // 计算可用 payload 容量
    uint8_t buffer_count = sender->config.buffer_count;
    uint16_t mtu = sender->config.mtu;
    uint16_t payload_max = mtu - sizeof(mp_header_t);

    uint8_t available_blocks = (buffer_count + sender->head - sender->tail - 1)
                                % buffer_count;

    pb_ostream_t stream = {
        .callback = MP_INTERNAL_StreamCallback,
        .state = &state,
        .max_size = (size_t)available_blocks * payload_max,
        .bytes_written = 0,
        .errmsg = 0
    };

    if (!pb_encode(&stream, fields, message)) {
        // 编码失败，回滚 head
        sender->head = old_head;
        return false;
    }

    // 提交最后一个可能未写满的 block
    if (state.head_in_slice > sizeof(mp_header_t))
        sender->head = (sender->head + 1) % buffer_count;

    // 统一回填所有 block 的帧头
    size_t total_bytes = stream.bytes_written;
    uint8_t slice_serial = 0;

    for (uint8_t blk = old_head; blk != sender->head; blk = (blk + 1) % buffer_count)
    {
        mp_header_t *header = (mp_header_t *)(sender->config.buffer
                                               + (size_t)blk * mtu);
        header->package_serial = sender->serial;
        header->slice_serial = slice_serial++;
        header->slice_payload_size = (total_bytes > payload_max)
                                     ? payload_max
                                     : (uint16_t)total_bytes;
        total_bytes -= header->slice_payload_size;
    }

    ++sender->serial;
    return true;
}