#include "mp-customdata-nanopb.h"
#include "mp-customdata.h"
#include <stdbool.h>
#include <string.h>
#include <pb_decode.h>

/* ================================================================
 * Decode stream callback — 从 ring buffer 顺序读取 payload
 * ================================================================ */
typedef struct {
    mp_receiver_t *receiver;
    uint8_t  block;      /* 当前正在读取的 block 下标 */
    uint16_t offset;     /* 当前 block 内的读取偏移 */
    uint8_t  end_block;  /* 包边界（head），读到即停止 */
} mp_decode_state_t;

static bool MP_INTERNAL_DecodeStreamCallback(
    pb_istream_t *stream, pb_byte_t *buf, size_t count)
{
    mp_decode_state_t *state = stream->state;
    mp_receiver_t *receiver = state->receiver;

    while (count > 0) {
        uint8_t next = (state->block + 1) % MP_RECEIVER_BUFFER_COUNT;
        bool is_last = (next == state->end_block);

        /* 当前 block 的实际 payload 大小 */
        size_t payload_size = is_last
            ? ((mp_header_t *)receiver->buffer[state->block])->slice_payload_size
            : (size_t)MP_RECEIVER_PAYLOAD_MAX_SIZE;

        /* 当前 block 剩余未读 payload 字节数 */
        size_t consumed  = state->offset - sizeof(mp_header_t);
        size_t available = payload_size - consumed;
        size_t to_read   = count < available ? count : available;

        memcpy(buf,
               &receiver->buffer[state->block][state->offset],
               to_read);

        buf   += to_read;
        count -= to_read;
        state->offset += to_read;

        /* 当前 block 已读完，切换到下一个 */
        if (state->offset >= sizeof(mp_header_t) + payload_size) {
            state->block  = next;
            state->offset = sizeof(mp_header_t);
        }
    }

    return true;
}

/* ================================================================
 * MP_Decode — nanopb 解码入口
 * ================================================================ */
bool MP_Decode(mp_receiver_t *receiver, const pb_msgdesc_t *fields, void *message)
{
    if (!receiver->package_complete)
        return false;

    /* 计算从 tail 到 head 之间所有 block 的总 payload 字节数 */
    size_t total = 0;
    uint8_t blk  = receiver->tail;

    while (blk != receiver->head) {
        uint8_t next = (blk + 1) % MP_RECEIVER_BUFFER_COUNT;

        if (next == receiver->head) {
            /* 末块：payload 大小由 slice_payload_size 指定 */
            total += ((mp_header_t *)receiver->buffer[blk])->slice_payload_size;
        } else {
            total += MP_RECEIVER_PAYLOAD_MAX_SIZE;
        }

        blk = next;
    }

    mp_decode_state_t state = {
        .receiver  = receiver,
        .block     = receiver->tail,
        .offset    = sizeof(mp_header_t),
        .end_block = receiver->head
    };

    pb_istream_t stream = {
        .callback   = MP_INTERNAL_DecodeStreamCallback,
        .state      = &state,
        .bytes_left = total,
        .errmsg     = NULL
    };

    if (!pb_decode(&stream, fields, message))
        return false;

    /* 解码成功 — 推进 tail，清除完成标志 */
    receiver->tail             = receiver->head;
    receiver->package_complete = false;

    return true;
}