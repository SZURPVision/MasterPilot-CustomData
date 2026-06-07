#ifndef __MP_CUSTOM_DATA_H_
#define __MP_CUSTOM_DATA_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma region Type Definition

typedef struct {
    uint8_t *buffer;        // 外部分配的一维 buffer. 注意大小不小于 buffer_count + mtu
    uint8_t  buffer_count;  // 分片数
    uint16_t mtu;           // 每个分片大小（含 header）
} mp_config_t;

/*
 * @brief 帧头定义
*/
#pragma pack(push,1)
typedef struct {
    // 大包序列号
    uint8_t package_serial;
    // 分片序列号
    uint8_t slice_serial;
    // 当前分片大小
    uint16_t slice_payload_size;
} mp_header_t;
#pragma pack(pop)

typedef struct {
    volatile uint8_t  head, tail;
    volatile uint16_t serial;   // 大包序列号
    const mp_config_t       config;
} mp_sender_t;

typedef struct {
    volatile uint8_t  tail;             // 已解码消费位置
    volatile uint8_t  head;             // 已收齐但未解码的包边界
    volatile uint8_t  slice_base;       // 当前大包起始块下标
    volatile uint8_t  slice_bitmap[32]; // 去重位图 (支持 buffer_count ≤ 256)
    volatile uint8_t  slices_received;  // 已接收的唯一分片数量
    volatile uint8_t  terminal_serial;  // 终止帧 slice_serial, 0xFF=未收到
    volatile bool     package_complete; // 是否有完整包待解码
    const mp_config_t       config;
} mp_receiver_t;

/*
 * @brief 流式 Block Writer — 封装分片/帧头写入逻辑
 */
typedef struct {
    mp_sender_t *sender;
    uint8_t  start_head;      // 起始 head（用于回滚/回填空帧头）
    uint8_t  current_head;    // 当前写入指针
    uint16_t offset_in_block; // 当前 block 内偏移
} mp_block_writer_t;

/*
 * @brief 流式 Block Reader — 封装分片/帧头读取逻辑
 */
typedef struct {
    mp_receiver_t *receiver;
    uint8_t  current_block;   // 当前读取 block 下标
    uint16_t offset_in_block; // 当前 block 内偏移
    uint8_t  end_block;       // 包边界（head），读到即停止
} mp_block_reader_t;


/*
 * @brief 消费者回调函数
 * @param source_block 需要读取的内存块
 * @param block_length 内存块剩余空间
 * @return 已读取的长度
 * @warning 注意不要越界.
*/
typedef uint16_t mp_consumer_cb_t(uint8_t* source_block, uint16_t block_length, void* user);

/*
 * @brief 从 ring buffer 逐 block 读出并通过 consumer 回调发送
 * @param consumer 硬件发送回调
 * @param user     透传给 consumer 的上下文
 * @return true 所有待发送 block 已成功发出
 */
bool MP_Send(mp_sender_t* sender, mp_consumer_cb_t consumer, void* user);

/*
 * @brief 接收一个完整data块(header + payload)并写入receiver
 */
bool MP_Receive(mp_receiver_t *receiver, const uint8_t *data);

/* ================================================================
 * 流式 Block Writer API — 封装分片/帧头逻辑
 * ================================================================ */

/*
 * @brief 初始化写入器，返回最大可写入 payload 字节数（不含帧头）
 */
uint16_t MP_BlockWriter_Begin(mp_block_writer_t *writer, mp_sender_t *sender);

/*
 * @brief 提取当前连续可写的内存指针, 返回可写长度, 自动处理分片
 * @return 连续可写的字节数
 */
uint16_t MP_BlockWriter_Acquire(mp_block_writer_t *writer, uint8_t **out_ptr);

/*
 * @brief 提交已写入的字节数, 以推动写入过程
 */
void MP_BlockWriter_CommitBytes(mp_block_writer_t *writer, const uint16_t length);

/*
 * @brief 返回已申请但未使用的字节
 */
void MP_BlockWriter_BackUp(mp_block_writer_t *writer, const uint16_t count);

/*
 * @brief 流式写入 payload 数据，自动处理跨块分片. 是多个拆散原语的简单封装.
 * @return 实际写入的字节数
 */
uint16_t MP_BlockWriter_Write(mp_block_writer_t *writer, const uint8_t*data, const uint16_t length);

/*
 * @brief 提交：推进 head、回填所有 block 的帧头、递增 serial
 * @param total_payload 编码出的总 payload 字节数
 */
void MP_BlockWriter_Commit(mp_block_writer_t *writer, const uint16_t total_payload);

/*
 * @brief 回滚：恢复 sender->head 到写入前的状态
 */
void MP_BlockWriter_Rollback(mp_block_writer_t *writer);

/* ================================================================
 * 流式 Block Reader API — 封装分片/帧头逻辑
 * ================================================================ */

/*
 * @brief 初始化读取器
 * @return 所有待读 block 的总 payload 字节数，若未收齐完整包则返回 0
 */
uint16_t MP_BlockReader_Begin(mp_block_reader_t *reader, mp_receiver_t *receiver);

/*
 * @brief 提取当前连续可写的内存指针, 返回可写长度, 自动处理分片
 * @return 连续可写的字节数
 */
uint16_t MP_BlockReader_Acquire(mp_block_reader_t *reader, const uint8_t **out_ptr);

/*
 * @brief 标记已消费的字节
 */
void MP_BlockReader_Advance(mp_block_reader_t *reader, const uint16_t length);

/*
 * @brief 流式读取 payload 数据，自动处理跨块和末块 payload_size 判断
 * @return 实际读取的字节数
 */
uint16_t MP_BlockReader_Read(mp_block_reader_t *reader, uint8_t *data, const uint16_t length);

/*
 * @brief 完成读取：推进 tail，清除 package_complete 标志
 */
void MP_BlockReader_Finish(mp_block_reader_t *reader);

#ifdef __cplusplus
}
#endif

#endif