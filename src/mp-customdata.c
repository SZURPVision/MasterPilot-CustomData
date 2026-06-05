#include "mp-customdata.h"
#include <string.h>

bool MP_Send(mp_sender_t *sender, mp_consumer_cb_t consumer, void *user)
{
    while (sender->tail != sender->head) {
        uint16_t sent = consumer(sender->buffer[sender->tail],
                                 MP_SENDER_MTU,
                                 user);
        if (sent != MP_SENDER_MTU)
            return false; /* 硬件发送失败，tail 不回退，可重试 */

        sender->tail = (sender->tail + 1) % MP_SENDER_BUFFER_COUNT;
    }

    return true; /* 所有待发 block 已发出 */
}

bool MP_Receive(mp_receiver_t *receiver, const uint8_t *data)
{
    const mp_header_t *header = (const mp_header_t *)data;

    /*
     * slice_serial 直接作为当前大包内的偏移下标写入 buffer
     * 写入位置 = slice_base + slice_serial，环形折返
     */
    uint8_t index = (receiver->slice_base + header->slice_serial)
                    % MP_RECEIVER_BUFFER_COUNT;

    memcpy(receiver->buffer[index], data, MP_RECEIVER_MTU);

    /* 更新已收分片计数（允许乱序到达） */
    if (header->slice_serial + 1 > receiver->slice_count)
        receiver->slice_count = header->slice_serial + 1;

    /* 末帧判定：payload 不满即为最后一个分片 */
    if (header->slice_payload_size < MP_RECEIVER_PAYLOAD_MAX_SIZE) {
        /* 标记当前大包边界并推进 slice_base 供下一个包使用 */
        receiver->head = (receiver->slice_base + receiver->slice_count)
                         % MP_RECEIVER_BUFFER_COUNT;
        receiver->package_complete = true;

        /* 为下一个大包准备起始位置 */
        receiver->slice_base = receiver->head;
        receiver->slice_count = 0;

        return true; /* 大包收齐 */
    }

    return false; /* 还需继续接收 */
}