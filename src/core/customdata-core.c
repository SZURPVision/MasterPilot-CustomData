#include <masterpilot/customdata-core.h>
#include <stddef.h>
#include <stdint.h>
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

    // 去重：bitmap 置位，首次出现递增 slices_received
    uint8_t byte_idx = header->slice_serial >> 3;
    uint8_t bit_mask = 1 << (header->slice_serial & 7);
    if (!(receiver->slice_bitmap[byte_idx] & bit_mask)) {
        receiver->slice_bitmap[byte_idx] |= bit_mask;
        ++receiver->slices_received;
    }

    // 末帧判定：payload 不满即为终止帧
    uint16_t payload_max = receiver->config.mtu - sizeof(mp_header_t);
    if (header->slice_payload_size < payload_max)
        receiver->terminal_serial = header->slice_serial;

    // 收齐：拿到终止帧 且 所有唯一分片均已到达
    if (receiver->terminal_serial != 0xFF
        && receiver->slices_received >= receiver->terminal_serial + 1)
    {
        receiver->head = (receiver->slice_base + receiver->terminal_serial + 1)
                         % receiver->config.buffer_count;
        receiver->package_complete = true;

        receiver->slice_base = receiver->head;
        memset((void *)receiver->slice_bitmap, 0, sizeof(receiver->slice_bitmap));
        receiver->slices_received = 0;
        receiver->terminal_serial = 0xFF;

        return true;
    }

    return false;
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

    uint8_t available_blocks = (buffer_count + sender->head - sender->tail - 2)
                                % buffer_count;
    return available_blocks * payload_max;
}

uint16_t MP_BlockWriter_Acquire(mp_block_writer_t *writer, uint8_t **out_ptr)
{
    uint16_t mtu = writer->sender->config.mtu;

    //没空间了就溜到下一个buffer
    if (writer->offset_in_block >= mtu)
    {
        writer->current_head = (writer->current_head + 1)% writer->sender->config.buffer_count;
        writer->offset_in_block = sizeof(mp_header_t);
    }

    *out_ptr = writer->sender->config.buffer + writer->current_head * mtu + writer->offset_in_block;
    return mtu - writer->offset_in_block;
}

void MP_BlockWriter_CommitBytes(mp_block_writer_t *writer, uint16_t length)
{
    writer->offset_in_block += length;
}

void MP_BlockWriter_BackUp(mp_block_writer_t* writer, const uint16_t count)
{
    writer->offset_in_block -= count;
}

uint16_t MP_BlockWriter_Write(mp_block_writer_t* restrict writer, const uint8_t* restrict data, const uint16_t length)
{
    uint16_t written = 0;

    while (written < length) {
        uint8_t* ptr = NULL;
        uint16_t avaliable = MP_BlockWriter_Acquire(writer, &ptr);

        if(avaliable == 0)break;

        uint16_t to_write = (length - written) < avaliable ? (length - written) : avaliable;
        memcpy(ptr, data + written, to_write);

        MP_BlockWriter_CommitBytes(writer, to_write);

        written += to_write;
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

    /*
     * 若 total_payload 恰好是 payload_max 的整数倍, 则最后一个数据 block 的
     * slice_payload_size == payload_max, receiver 无法区分末帧.
     * 追加一个 slice_payload_size=0 的空帧作为终止标记.
     * Begin() 已预留 1 个空闲 block, 此处必然有空位.
     */
    if (total_payload > 0 && total_payload % payload_max == 0) {
        uint8_t *block = sender->config.buffer + (size_t)sender->head * mtu;
        mp_header_t *header = (mp_header_t *)block;
        memset(block, 0, mtu);
        header->package_serial    = sender->serial;
        header->slice_serial      = slice_serial;
        header->slice_payload_size = 0;

        sender->head = (sender->head + 1) % buffer_count;
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

uint16_t MP_BlockReader_Acquire(mp_block_reader_t *reader, const uint8_t **out_ptr)
{
    mp_receiver_t *receiver = reader->receiver;
    uint16_t mtu = receiver->config.mtu;
    uint8_t buffer_count = receiver->config.buffer_count;
    
    if (reader->offset_in_block >= mtu)
    {
        reader->current_block = (reader->current_block + 1) % buffer_count;
        reader->offset_in_block = sizeof(mp_header_t);
    }

    bool is_last = (((reader->current_block + 1) % buffer_count) == reader->end_block);
    size_t payload_size = is_last 
        ? ((mp_header_t *)(receiver->config.buffer + (size_t)reader->current_block * mtu))->slice_payload_size
        : (mtu - sizeof(mp_header_t));

    if (reader->offset_in_block - sizeof(mp_header_t) >= payload_size) {
        return 0;
    }

    *out_ptr = receiver->config.buffer 
               + (size_t)reader->current_block * mtu 
               + reader->offset_in_block;
               
    return (uint16_t)(payload_size - (reader->offset_in_block - sizeof(mp_header_t)));
}

void MP_BlockReader_Advance(mp_block_reader_t *reader, const uint16_t length) {
    reader->offset_in_block += length;
}

uint16_t MP_BlockReader_Read(mp_block_reader_t *reader, uint8_t *data, const uint16_t length)
{
    //我真是服了, read的完成时还叫read
    uint16_t red = 0;

    while (red < length)
    {
        const uint8_t* ptr = NULL;
        uint16_t avaliable = MP_BlockReader_Acquire(reader, &ptr);
        
        if(avaliable == 0) break;
        
        uint16_t to_read = (length - red) < avaliable ? (length - red) : avaliable;
        memcpy(data + red, ptr, to_read);

        MP_BlockReader_Advance(reader, to_read);
        red += to_read;
    }
    
    return red;
}

void MP_BlockReader_Finish(mp_block_reader_t *reader)
{
    reader->receiver->tail             = reader->receiver->head;
    reader->receiver->package_complete = false;
}
