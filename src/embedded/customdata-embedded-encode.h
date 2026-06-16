#ifndef __MP_CUSTOM_DATA_EMBEDDED_ENCODE_H_
#define __MP_CUSTOM_DATA_EMBEDDED_ENCODE_H_

#include "masterpilot/customdata-stream-tx.h"
#include <masterpilot/customdata-common.h>
#include <pb.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将自定义数据传输层与`nanopb`编码二合一的实例
 * 
 */
typedef struct 
{
    void* user;
    const mp_config_t config;
    const uint8_t sender_id; /**< 不冲突就行. 电控建议填0 */
    void (*data_put_cb)(void* user, const uint16_t offset, const uint8_t* block, uint16_t size); /**< 发送缓冲区写操作回调. 直接写裁判系统缓冲区的`data`部分. CRC校验可以在这里进行. */
    void (*send_signal_cb)(void* user); /**< 进行实际的发送操作, 这个版本应该要实现进行CRC包尾追加+串口发送. 此次回调完成后, 会覆盖缓冲区. 同步阻塞式. */
    
    /* 内部状态, 无需手动初始化 */

    mp_tx_stream_t mp_stream; /**< 封装的tx流 */
    uint16_t accumulated; /**< 裁判系统data缓冲区中payload的填充状态 */
} mp_pb_encoder_inst_t;

/**
 * @brief 直接从nanopb结构体编码到流式输出口(串口缓冲区)
 * @param fields nanopb生成的消息名_fields结构体
 * @param message 要发送的 nanopb消息结构体
 * @param package_serial 包序列号. 发完需要自行自增.
 * @return 编码是否成功. 与发送无关.
*/
bool mp_pb_encode(mp_pb_encoder_inst_t *instance, const pb_msgdesc_t *fields, const void *message,const uint8_t package_serial);

#ifdef __cplusplus
}
#endif

#endif