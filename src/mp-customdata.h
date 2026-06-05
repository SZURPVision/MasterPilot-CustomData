#ifndef __MP_CUSTOM_DATA_H_
#define __MP_CUSTOM_DATA_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma region Constant Definition

/* 发送端配置 */
#ifndef MP_SENDER_BUFFER_COUNT
#define MP_SENDER_BUFFER_COUNT 16
#endif

#ifndef MP_SENDER_MTU
#define MP_SENDER_MTU 300
#endif

#define MP_SENDER_PAYLOAD_MAX_SIZE (MP_SENDER_MTU - sizeof(mp_header_t))

/* 接收端配置 */
#ifndef MP_RECEIVER_BUFFER_COUNT
#define MP_RECEIVER_BUFFER_COUNT 16
#endif

#ifndef MP_RECEIVER_MTU
#define MP_RECEIVER_MTU 30
#endif

#define MP_RECEIVER_PAYLOAD_MAX_SIZE (MP_RECEIVER_MTU - sizeof(mp_header_t))



#pragma endregion

/*
 * @brief 帧头定义
*/
#pragma pack(push)
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
    volatile uint8_t head, tail;
    volatile uint16_t serial; //大包序列号
    uint8_t buffer[MP_SENDER_BUFFER_COUNT][MP_SENDER_MTU];
} mp_sender_t;

typedef struct {
    volatile uint8_t tail;              /* 已解码消费位置 */
    volatile uint8_t head;              /* 已收齐但未解码的包边界 */
    volatile uint8_t slice_base;        /* 当前大包起始块下标 */
    volatile uint8_t slice_count;       /* 当前大包已收分片数 */
    volatile bool   package_complete;   /* 是否有完整包待解码 */
    uint8_t buffer[MP_RECEIVER_BUFFER_COUNT][MP_RECEIVER_MTU];
} mp_receiver_t;


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

#ifdef __cplusplus
}
#endif

#endif