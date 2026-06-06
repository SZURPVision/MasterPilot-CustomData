#include "customdata-core.h"
#include <string.h>

bool MP_Send(mp_sender_t *sender, mp_consumer_cb_t consumer, void *user)
{
    while (sender->tail != sender->head) {
        uint8_t *block = sender->config.buffer
                         + (size_t)sender->tail * sender->config.mtu;
        uint16_t sent = consumer(block, sender->config.mtu, user);
        if (sent != sender->config.mtu)
            return false; // 硬件发送失败，tail 不回退，可重试

        sender->tail = (sender->tail + 1) % sender->config.buffer_count;
    }

    return true; // 所有待发 block 已发出
}

bool MP_Receive(mp_receiver_t *receiver, const uint8_t *data)
{
    const mp_header_t *header = (const mp_header_t *)data;

    /*
     * slice_serial 直接作为当前大包内的偏移下标写入 buffer
     * 写入位置 = slice_base + slice_serial，环形折返
     */
    uint8_t index = (receiver->slice_base + header->slice_serial)
                    % receiver->config.buffer_count;

    memcpy(receiver->config.buffer + (size_t)index * receiver->config.mtu,
           data, receiver->config.mtu);

    // 更新已收分片计数（允许乱序到达）
    if (header->slice_serial + 1 > receiver->slice_count)
        receiver->slice_count = header->slice_serial + 1;

    // 末帧判定：payload 不满即为最后一个分片
    uint16_t payload_max = receiver->config.mtu - sizeof(mp_header_t);
    if (header->slice_payload_size < payload_max) {
        // 标记当前大包边界并推进 slice_base 供下一个包使用
        receiver->head = (receiver->slice_base + receiver->slice_count)
                         % receiver->config.buffer_count;
        receiver->package_complete = true;

        // 为下一个大包准备起始位置
        receiver->slice_base = receiver->head;
        receiver->slice_count = 0;

        return true; // 大包收齐
    }

    return false; // 还需继续接收
}

/* ================================================================
 * 流式 Block Writer 实现
 * ================================================================ */

uint16_t MP_BlockWriter_Begin(mp_block_writer_t *writer, mp_sender_t *sender)
{
    uint8_t buffer_count = sender->config.buffer_count;
    uint16_t mtu = sender->config.mtu;
    uint16_t payload_max = mtu - sizeof(mp_header_t);

    writer->sender       = sender;
    writer->start_head   = sender->head;
    writer->current_head = sender->head;
    writer->offset_in_block = sizeof(mp_header_t);

    uint8_t available_blocks = (buffer_count + sender->head - sender->tail - 1)
                                % buffer_count;
    return available_blocks * payload_max;
}

uint16_t MP_BlockWriter_Write(mp_block_writer_t *writer, const uint8_t *data, uint16_t length)
{
    uint16_t mtu = writer->sender->config.mtu;
    uint8_t  buffer_count = writer->sender->config.buffer_count;
    uint16_t written = 0;

    while (length > 0) {
        size_t available = mtu - writer->offset_in_block;
        size_t to_write  = length < available ? length : available;

        uint8_t *block = writer->sender->config.buffer
                         + (size_t)writer->current_head * mtu;
        memcpy(&block[writer->offset_in_block], data, to_write);

        data   += to_write;
        length -= to_write;
        writer->offset_in_block += (uint16_t)to_write;
        written += (uint16_t)to_write;

        if (writer->offset_in_block >= mtu) {
            writer->current_head = (writer->current_head + 1) % buffer_count;
            writer->offset_in_block = sizeof(mp_header_t);
        }
    }

    return written;
}

void MP_BlockWriter_Commit(mp_block_writer_t *writer, uint16_t total_payload)
{
    mp_sender_t *sender = writer->sender;
    uint16_t mtu = sender->config.mtu;
    uint16_t payload_max = mtu - sizeof(mp_header_t);
    uint8_t buffer_count = sender->config.buffer_count;

    /* 提交最后一个可能未写满的 block */
    if (writer->offset_in_block > sizeof(mp_header_t))
        writer->current_head = (writer->current_head + 1) % buffer_count;

    sender->head = writer->current_head;

    /* 回填所有 block 的帧头 */
    uint16_t remaining = total_payload;
    uint8_t  slice_serial = 0;

    for (uint8_t blk = writer->start_head;
         blk != sender->head;
         blk = (blk + 1) % buffer_count)
    {
        mp_header_t *header = (mp_header_t *)(sender->config.buffer
                                               + (size_t)blk * mtu);
        header->package_serial = sender->serial;
        header->slice_serial = slice_serial++;
        header->slice_payload_size = (remaining > payload_max)
                                      ? payload_max
                                      : remaining;
        remaining -= header->slice_payload_size;
    }

    ++sender->serial;
}

void MP_BlockWriter_Rollback(mp_block_writer_t *writer)
{
    writer->sender->head = writer->start_head;
}

/* ================================================================
 * 流式 Block Reader 实现
 * ================================================================ */

uint16_t MP_BlockReader_Begin(mp_block_reader_t *reader, mp_receiver_t *receiver)
{
    if (!receiver->package_complete)
        return 0;

    uint16_t mtu = receiver->config.mtu;
    uint16_t payload_max = mtu - sizeof(mp_header_t);
    uint8_t buffer_count = receiver->config.buffer_count;

    /* 计算从 tail 到 head 之间所有 block 的总 payload 字节数 */
    uint16_t total = 0;
    uint8_t  blk   = receiver->tail;

    while (blk != receiver->head) {
        uint8_t next = (blk + 1) % buffer_count;

        if (next == receiver->head) {
            /* 末块：payload 大小由 slice_payload_size 指定 */
            total += ((mp_header_t *)(receiver->config.buffer
                                       + (size_t)blk * mtu))->slice_payload_size;
        } else {
            total += payload_max;
        }

        blk = next;
    }

    reader->receiver       = receiver;
    reader->current_block   = receiver->tail;
    reader->offset_in_block = sizeof(mp_header_t);
    reader->end_block       = receiver->head;

    return total;
}

uint16_t MP_BlockReader_Read(mp_block_reader_t *reader, uint8_t *data, uint16_t length)
{
    mp_receiver_t *receiver = reader->receiver;
    uint16_t mtu = receiver->config.mtu;
    uint16_t payload_max = mtu - sizeof(mp_header_t);
    uint8_t  buffer_count = receiver->config.buffer_count;
    uint16_t read_total = 0;

    while (length > 0) {
        uint8_t next = (reader->current_block + 1) % buffer_count;
        bool is_last = (next == reader->end_block);

        /* 当前 block 的实际 payload 大小 */
        size_t payload_size = is_last
            ? ((mp_header_t *)(receiver->config.buffer
                                + (size_t)reader->current_block * mtu))->slice_payload_size
            : (size_t)payload_max;

        /* 当前 block 剩余未读 payload 字节数 */
        size_t consumed  = reader->offset_in_block - sizeof(mp_header_t);
        size_t available = payload_size - consumed;
        size_t to_read   = length < available ? length : available;

        memcpy(data,
               receiver->config.buffer + (size_t)reader->current_block * mtu
                                        + reader->offset_in_block,
               to_read);

        data   += to_read;
        length -= to_read;
        reader->offset_in_block += (uint16_t)to_read;
        read_total += (uint16_t)to_read;

        /* 当前 block 已读完，切换到下一个 */
        if (reader->offset_in_block >= sizeof(mp_header_t) + payload_size) {
            reader->current_block  = next;
            reader->offset_in_block = sizeof(mp_header_t);
        }
    }

    return read_total;
}

void MP_BlockReader_Finish(mp_block_reader_t *reader)
{
    reader->receiver->tail             = reader->receiver->head;
    reader->receiver->package_complete = false;
}
